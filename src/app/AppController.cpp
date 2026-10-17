#include "AppController.h"

#include "core/CodeGenerator.h"
#include "core/Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#ifndef P2P_API_BASE
#define P2P_API_BASE ""
#endif
#ifndef P2P_VERSION
#define P2P_VERSION "0.0.0"
#endif

namespace p2p::app {

using core::CodeGenerator;
using core::Identity;
using network::ApiResult;

namespace {
constexpr int kMaxClaimAttempts = 20;
constexpr int kRetryMs = 5000;

QString dataFile(const QString &name)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return dir + QLatin1Char('/') + name;
}
} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_engine(&m_bridge, &m_transfers)
    , m_history(dataFile(QStringLiteral("history.json")))
    , m_blocked(dataFile(QStringLiteral("blocked.json")))
{
    QSettings settings;
    const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
        + QLatin1Char('/') + QCoreApplication::applicationName();
    m_engine.setDownloadDir(settings.value(QStringLiteral("downloadDir"), defaultDir).toString());

    connect(&m_client, &network::CloudflareClient::notification, this, &AppController::onNotification);
    connect(&m_client, &network::CloudflareClient::socketStateChanged, this, [this](bool up) {
        m_socketUp = up;
        setOnline(up && m_peerOpen);
        if (up)
            refreshInbox(); // pick up anything that arrived while disconnected
        else if (m_peerOpen)
            setStatus(tr("Reconnecting…"));
    });

    connect(&m_bridge, &transfer::PeerBridge::peerOpened, this, [this](const QString &id) {
        m_peerOpen = true;
        m_client.registerSession(id, [this](const ApiResult &r) {
            if (r.ok()) {
                m_client.openSocket();
                refreshInbox();
                setStatus(tr("Ready"));
            } else if (r.status == 401) {
                // Our code was released or expired server-side: start over with a new one.
                Identity::clear();
                m_identity = {};
                emit localCodeChanged();
                claimNewCode(0);
            } else {
                setStatus(tr("Cannot register with the server: %1").arg(r.error));
            }
        });
    });
    connect(&m_bridge, &transfer::PeerBridge::peerDisconnected, this, [this] {
        m_peerOpen = false;
        setOnline(false);
        QTimer::singleShot(3000, &m_bridge, &transfer::PeerBridge::reconnectSignaling);
    });
    connect(&m_bridge, &transfer::PeerBridge::peerError, this, [this](const QString &msg) {
        setStatus(tr("Peer network error: %1").arg(msg));
    });

    connect(&m_engine, &transfer::TransferEngine::statusMessage, this, &AppController::setStatus);
    connect(&m_transfers, &models::TransferModel::finished, this,
            [this](const QString &name, qint64 size, bool sending, const QString &peer,
                   const QString &state, const QString &error, const QString &path) {
                models::HistoryModel::Entry e;
                e.name = name;
                e.size = size;
                e.sending = sending;
                e.peer = peer;
                e.result = state;
                e.error = error;
                e.path = path;
                e.when = QDateTime::currentDateTime();
                m_history.add(e);
            });
    connect(&m_updates, &network::UpdateChecker::updateAvailable, this,
            [this](const QString &version, const QUrl &url) {
                m_updateVersion = version;
                m_updateUrl = url;
                emit updateVersionChanged();
            });
    connect(&m_transfers, &models::TransferModel::activeCountChanged, this,
            [this] { setBusy(m_transfers.activeCount() > 0); });
}

void AppController::setStatus(const QString &text)
{
    if (m_status == text)
        return;
    m_status = text;
    emit statusChanged();
}

void AppController::setOnline(bool online)
{
    if (m_online == online)
        return;
    m_online = online;
    emit onlineChanged();
}

void AppController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void AppController::start()
{
    QString base = qEnvironmentVariable("MODERNP2P_API");
    if (base.isEmpty())
        base = QSettings().value(QStringLiteral("apiBase")).toString();
    if (base.isEmpty())
        base = QStringLiteral(P2P_API_BASE);
    m_client.setBaseUrl(QUrl(base));

    if (!m_client.isConfigured()) {
        setStatus(tr("Server URL is not configured. Set MODERNP2P_API or rebuild with P2P_API_BASE."));
        return;
    }

    QTimer::singleShot(8000, this, [this] { m_updates.check(version()); });

    const Identity saved = Identity::load();
    if (saved.isValid() && CodeGenerator::isValid(saved.code))
        useIdentity(saved);
    else
        claimNewCode(0);
}

void AppController::claimNewCode(int attempt)
{
    if (attempt >= kMaxClaimAttempts) {
        setStatus(tr("Could not register a code. Check your connection."));
        return;
    }
    setStatus(tr("Registering your code…"));
    const QString code = CodeGenerator::generate();
    m_client.claim(code, [this, code, attempt](const ApiResult &r) {
        if (r.ok()) {
            useIdentity({code, r.json.value(QStringLiteral("ownerToken")).toString()});
            m_identity.save();
        } else if (r.status == 409) {
            claimNewCode(attempt + 1);                       // taken: roll a new one
        } else {
            setStatus(tr("Cannot reach the server: %1").arg(r.error));
            QTimer::singleShot(kRetryMs, this, [this, attempt] { claimNewCode(attempt); });
        }
    });
}

void AppController::useIdentity(const Identity &id)
{
    m_identity = id;
    m_client.setCredentials(id.code, id.token);
    emit localCodeChanged();
    startSession();
}

void AppController::startSession()
{
    m_peerOpen = false;
    setOnline(false);
    m_peerId = CodeGenerator::makePeerId(m_identity.code);
    setStatus(tr("Connecting to the peer network…"));
    m_bridge.start(m_peerId);
}

void AppController::refreshInbox()
{
    m_client.fetchInbox([this](const ApiResult &r) {
        if (!r.ok())
            return;
        for (const QJsonValue &v : r.json.value(QStringLiteral("requests")).toArray())
            takeIncoming(v.toObject());
    });
}

bool AppController::takeIncoming(const QJsonObject &request)
{
    const QString id = request.value(QStringLiteral("id")).toString();
    if (m_blocked.contains(request.value(QStringLiteral("from")).toString())) {
        if (!id.isEmpty())
            m_client.respond(id, false, [](const ApiResult &) {});
        return false;
    }
    if (!m_requests.add(request))
        return false;
    emit requestArrived();
    return true;
}

void AppController::onNotification(const QJsonObject &msg)
{
    const QString type = msg.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("request")) {
        takeIncoming(msg.value(QStringLiteral("request")).toObject());
    } else if (type == QLatin1String("response")) {
        const QString reqId = msg.value(QStringLiteral("requestId")).toString();
        const auto it = m_outgoing.find(reqId);
        if (it == m_outgoing.end())
            return;
        const Outgoing out = it.value();
        m_outgoing.erase(it);

        if (!msg.value(QStringLiteral("accepted")).toBool()) {
            setStatus(tr("%1 declined the request").arg(out.toCode));
            return;
        }
        m_staging.clear();
        m_engine.startSend(reqId, msg.value(QStringLiteral("peerId")).toString(), out.toCode, out.paths, out.names);
    }
}

void AppController::connectToPeer(const QString &rawCode)
{
    const QString code = CodeGenerator::normalize(rawCode);
    if (!CodeGenerator::isValid(code))
        return setStatus(tr("A code is 8 letters or digits"));
    if (code == m_identity.code)
        return setStatus(tr("That is your own code"));
    if (m_staging.count() == 0)
        return setStatus(tr("Drop files into the staging area first"));
    if (!m_online)
        return setStatus(tr("You are offline"));

    setStatus(tr("Looking up %1…").arg(code));
    const QStringList paths = m_staging.paths();
    const QStringList names = m_staging.names();
    const QJsonArray files = m_staging.toJson();

    m_client.lookup(code, [this, code, paths, names, files](const ApiResult &r) {
        if (!r.ok())
            return setStatus(tr("Lookup failed: %1").arg(r.error));
        if (!r.json.value(QStringLiteral("exists")).toBool())
            return setStatus(tr("No device with code %1").arg(code));
        if (!r.json.value(QStringLiteral("online")).toBool())
            return setStatus(tr("%1 is offline").arg(code));

        m_client.sendRequest(code, files, [this, code, paths, names](const ApiResult &s) {
            if (!s.ok())
                return setStatus(tr("Request failed: %1").arg(s.error));
            m_outgoing.insert(s.json.value(QStringLiteral("id")).toString(), {paths, names, code});
            setStatus(tr("Request sent to %1. Waiting for them to accept…").arg(code));
        });
    });
}

void AppController::acceptRequest(const QString &requestId)
{
    const QJsonObject req = m_requests.take(requestId);
    if (req.isEmpty())
        return;

    QList<transfer::FileSpec> files;
    for (const QJsonValue &v : req.value(QStringLiteral("files")).toArray()) {
        const QJsonObject f = v.toObject();
        files.append({f.value(QStringLiteral("name")).toString(),
                      static_cast<qint64>(f.value(QStringLiteral("size")).toDouble())});
    }
    // Register before replying so the sender can never connect ahead of us.
    m_engine.expectIncoming(requestId, req.value(QStringLiteral("from")).toString(),
                            req.value(QStringLiteral("fromPeerId")).toString(), files);
    m_client.respond(requestId, true, [this](const ApiResult &r) {
        if (!r.ok())
            setStatus(tr("Could not accept: %1").arg(r.error));
    });
}

void AppController::rejectRequest(const QString &requestId)
{
    if (m_requests.take(requestId).isEmpty())
        return;
    m_client.respond(requestId, false, [](const ApiResult &) {});
}

void AppController::blockRequest(const QString &requestId)
{
    const QJsonObject req = m_requests.take(requestId);
    if (req.isEmpty())
        return;
    m_blocked.add(req.value(QStringLiteral("from")).toString());
    m_client.respond(requestId, false, [](const ApiResult &) {});
    setStatus(tr("Blocked %1").arg(req.value(QStringLiteral("from")).toString()));
}

void AppController::showInFolder(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.exists())
        return setStatus(tr("The file no longer exists"));
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            {QStringLiteral("/select,"), QDir::toNativeSeparators(fi.absoluteFilePath())});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
#endif
}

void AppController::openLogFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(core::Logger::directory()));
}

void AppController::openUpdatePage()
{
    if (m_updateUrl.isValid())
        QDesktopServices::openUrl(m_updateUrl);
}

QString AppController::profile() const
{
    const QString name = QCoreApplication::applicationName();
    return name.startsWith(QLatin1String("ModernP2P-")) ? name.mid(10) : QString();
}

QString AppController::version() const
{
    return QStringLiteral(P2P_VERSION);
}

void AppController::regenerateCode()
{
    if (m_busy)
        return setStatus(tr("Wait for the current transfers to finish first"));
    m_client.closeSocket();
    setOnline(false);
    m_client.release([this](const ApiResult &) {
        Identity::clear();
        m_identity = {};
        m_peerOpen = false;
        emit localCodeChanged();
        claimNewCode(0);
    });
}

void AppController::cancelTransfer(const QString &transferId)
{
    m_engine.cancel(transferId);
}

void AppController::addFiles(const QList<QUrl> &urls)
{
    QStringList paths;
    for (const QUrl &u : urls) {
        if (u.isLocalFile())
            paths << u.toLocalFile();
    }
    const int skipped = m_staging.addPaths(paths);
    if (skipped > 0)
        setStatus(tr("Skipped %n item(s): some items could not be added (duplicates, unreadable or too many)", nullptr, skipped));
}

void AppController::setDownloadDir(const QUrl &folder)
{
    if (!folder.isLocalFile())
        return;
    const QString dir = folder.toLocalFile();
    QSettings().setValue(QStringLiteral("downloadDir"), dir);
    m_engine.setDownloadDir(dir);
    emit downloadDirChanged();
}

} // namespace p2p::app
