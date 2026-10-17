#include "PeerBridge.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QDebug>
#include <utility>

namespace p2p::transfer {

namespace {

/// A JSON string literal, safe to embed in generated JavaScript.
QString js(const QString &s)
{
    const QString arr = QString::fromUtf8(QJsonDocument(QJsonArray{s}).toJson(QJsonDocument::Compact));
    return arr.mid(1, arr.size() - 2);
}

class BridgePage : public QWebEnginePage
{
public:
    using QWebEnginePage::QWebEnginePage;

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel, const QString &message, int line,
                                  const QString &source) override
    {
        qWarning().noquote() << "[bridge]" << message << source << line;
    }
};

} // namespace

PeerBridge::PeerBridge(QObject *parent)
    : PeerTransport(parent)
{
    m_profile = new QWebEngineProfile(this); // off-the-record: nothing is written to disk
    m_page = new BridgePage(m_profile, this);
    m_page->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    m_page->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);

    m_channel = new QWebChannel(this);
    m_channel->registerObject(QStringLiteral("host"), this);
    m_page->setWebChannel(m_channel);

    connect(m_page, &QWebEnginePage::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus, int) {
                emit peerError(QStringLiteral("Browser engine crashed"));
            });

    m_signalling = signallingFromEnvironment();
    m_page->load(QUrl(QStringLiteral("qrc:/web/bridge.html")));
}

PeerBridge::~PeerBridge() = default;

QJsonObject PeerBridge::signallingFromEnvironment()
{
    QString spec = qEnvironmentVariable("MODERNP2P_PEERJS").trimmed();
    if (spec.isEmpty())
        return {};
    bool secure = false;
    if (spec.startsWith(QLatin1String("https://"))) {
        secure = true;
        spec.remove(0, 8);
    } else if (spec.startsWith(QLatin1String("http://"))) {
        spec.remove(0, 7);
    }
    QString path = QStringLiteral("/");
    const qsizetype slash = spec.indexOf(QLatin1Char('/'));
    if (slash >= 0) {
        path = spec.mid(slash);
        spec = spec.left(slash);
    }
    QString host = spec;
    int port = secure ? 443 : 80;
    const qsizetype colon = spec.lastIndexOf(QLatin1Char(':'));
    if (colon > 0) {
        host = spec.left(colon);
        port = spec.mid(colon + 1).toInt();
    }
    if (host.isEmpty() || port <= 0)
        return {};
    return QJsonObject{{QStringLiteral("host"), host}, {QStringLiteral("port"), port},
                       {QStringLiteral("path"), path}, {QStringLiteral("secure"), secure}};
}

void PeerBridge::run(const QString &script)
{
    if (!m_ready) {
        m_pending << script;
        return;
    }
    m_page->runJavaScript(script);
}

void PeerBridge::start(const QString &peerId)
{
    const QString options = QString::fromUtf8(QJsonDocument(m_signalling).toJson(QJsonDocument::Compact));
    run(QStringLiteral("bridge.start(%1,%2)").arg(js(peerId), js(options)));
}

void PeerBridge::reconnectSignaling()
{
    run(QStringLiteral("bridge.reconnect()"));
}

void PeerBridge::connectTo(const QString &cid, const QString &remotePeerId, const QJsonObject &metadata)
{
    const QString meta = QString::fromUtf8(QJsonDocument(metadata).toJson(QJsonDocument::Compact));
    run(QStringLiteral("bridge.connectTo(%1,%2,%3)").arg(js(cid), js(remotePeerId), js(meta)));
}

void PeerBridge::send(const QString &cid, const QString &message, int seq)
{
    run(QStringLiteral("bridge.send(%1,%2,%3)").arg(js(cid), js(message)).arg(seq));
}

void PeerBridge::closeConnection(const QString &cid)
{
    run(QStringLiteral("bridge.close(%1)").arg(js(cid)));
}

void PeerBridge::jsReady()
{
    m_ready = true;
    const QStringList pending = std::exchange(m_pending, {});
    for (const QString &script : pending)
        m_page->runJavaScript(script);
}

void PeerBridge::jsPeerOpen(const QString &id) { emit peerOpened(id); }
void PeerBridge::jsPeerDisconnected() { emit peerDisconnected(); }
void PeerBridge::jsPeerError(const QString &message) { emit peerError(message); }

void PeerBridge::jsIncoming(const QString &cid, const QString &remotePeerId, const QString &metadataJson)
{
    emit incomingConnection(cid, remotePeerId, QJsonDocument::fromJson(metadataJson.toUtf8()).object());
}

void PeerBridge::jsConnOpen(const QString &cid) { emit connectionOpened(cid); }
void PeerBridge::jsConnClosed(const QString &cid) { emit connectionClosed(cid); }
void PeerBridge::jsConnError(const QString &cid, const QString &message) { emit connectionError(cid, message); }
void PeerBridge::jsMessage(const QString &cid, const QString &message) { emit messageReceived(cid, message); }
void PeerBridge::jsSent(const QString &cid, int seq) { emit messageSent(cid, seq); }

} // namespace p2p::transfer
