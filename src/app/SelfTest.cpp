#include "SelfTest.h"

#include "core/CodeGenerator.h"
#include "models/TransferModel.h"
#include "network/CloudflareClient.h"
#include "transfer/PeerBridge.h"
#include "transfer/TransferEngine.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>
#include <functional>
#include <memory>

#ifndef P2P_API_BASE
#define P2P_API_BASE ""
#endif

namespace p2p::app {

using core::CodeGenerator;
using models::TransferModel;
using network::ApiResult;
using network::CloudflareClient;
using transfer::PeerBridge;
using transfer::TransferEngine;

QString compiledApiBase()
{
    const QString env = qEnvironmentVariable("MODERNP2P_API");
    return env.isEmpty() ? QStringLiteral(P2P_API_BASE) : env;
}

namespace {

bool waitUntil(const std::function<bool()> &condition, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > timeoutMs)
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(5);
    }
    return true;
}

QByteArray randomBytes(qint64 n)
{
    QByteArray b(n, Qt::Uninitialized);
    for (qint64 i = 0; i < n; ++i)
        b[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
    return b;
}

QString writeFile(const QDir &dir, const QString &rel, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(dir.filePath(rel)).absolutePath());
    QFile f(dir.filePath(rel));
    if (f.open(QIODevice::WriteOnly))
        f.write(data);
    return f.fileName();
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

struct Pending
{
    bool done = false;
    ApiResult result;
};

} // namespace

int runSelfTest(const QString &outPath, const QString &apiBase)
{
    QFile out(outPath);
    out.open(QIODevice::WriteOnly | QIODevice::Truncate);
    bool ok = true;
    auto log = [&](const QString &line) {
        out.write(line.toUtf8() + "\n");
        out.flush();
        qInfo().noquote() << "[selftest]" << line;
    };
    auto pass = [&](const QString &what) { log(QStringLiteral("OK   ") + what); };
    auto fail = [&](const QString &what) {
        ok = false;
        log(QStringLiteral("FAIL ") + what);
    };

    // ---- the two devices ---------------------------------------------------
    PeerBridge bridgeA, bridgeB;
    TransferModel modelA, modelB;
    TransferEngine engineA(&bridgeA, &modelA), engineB(&bridgeB, &modelB);
    CloudflareClient clientA, clientB;

    QTemporaryDir src, dst;
    engineB.setDownloadDir(dst.path());

    const bool useWorker = !apiBase.isEmpty();
    QString codeA = QStringLiteral("AAAAAAAA"), codeB = QStringLiteral("BBBBBBBB");
    QString tokenA, tokenB;
    QString lastError;

    // A call to the Worker, waited for with a timeout. The state is shared so a
    // late callback can never touch freed memory.
    auto call = [&](const std::function<void(CloudflareClient::Callback)> &invoke, int ms = 20000) {
        auto state = std::make_shared<Pending>();
        invoke([state](const ApiResult &r) {
            state->result = r;
            state->done = true;
        });
        if (!waitUntil([&] { return state->done; }, ms)) {
            ApiResult timeout;
            timeout.error = QStringLiteral("timed out");
            return timeout;
        }
        return state->result;
    };

    auto finish = [&]() -> int {
        if (useWorker) {
            clientA.closeSocket();
            clientB.closeSocket();
            if (!tokenA.isEmpty())
                call([&](CloudflareClient::Callback cb) { clientA.release(cb); }, 10000);
            if (!tokenB.isEmpty())
                call([&](CloudflareClient::Callback cb) { clientB.release(cb); }, 10000);
        }
        log(ok ? QStringLiteral("RESULT: all checks passed") : QStringLiteral("RESULT: FAILED"));
        return ok ? 0 : 1;
    };

    log(QStringLiteral("ModernP2P self-test, Worker: %1").arg(useWorker ? apiBase : QStringLiteral("not configured")));

    // ---- 1. claim two codes on the Worker ------------------------------------
    auto claim = [&](CloudflareClient &client, QString &code, QString &token) {
        for (int attempt = 0; attempt < 5; ++attempt) {
            code = CodeGenerator::generate();
            const ApiResult r = call([&](CloudflareClient::Callback cb) { client.claim(code, cb); });
            if (r.ok()) {
                token = r.json.value(QStringLiteral("ownerToken")).toString();
                client.setCredentials(code, token);
                return !token.isEmpty();
            }
            lastError = r.error;
            if (r.status != 409)
                return false;
        }
        return false;
    };

    if (useWorker) {
        clientA.setBaseUrl(QUrl(apiBase));
        clientB.setBaseUrl(QUrl(apiBase));
        if (!claim(clientA, codeA, tokenA) || !claim(clientB, codeB, tokenB)) {
            fail(QStringLiteral("claiming codes on the Worker: %1").arg(lastError));
            return finish();
        }
        pass(QStringLiteral("claimed codes %1 and %2 on the Worker").arg(codeA, codeB));
    } else {
        log(QStringLiteral("SKIP Worker checks (no server URL configured in this build)"));
    }

    // ---- 2. both peers register with PeerJS ------------------------------------
    const QString idA = CodeGenerator::makePeerId(codeA), idB = CodeGenerator::makePeerId(codeB);
    bool openA = false, openB = false;
    QString peerError;
    QObject::connect(&bridgeA, &PeerBridge::peerOpened, &bridgeA, [&](const QString &) { openA = true; });
    QObject::connect(&bridgeB, &PeerBridge::peerOpened, &bridgeB, [&](const QString &) { openB = true; });
    QObject::connect(&bridgeA, &PeerBridge::peerError, &bridgeA, [&](const QString &m) { peerError = m; });
    QObject::connect(&bridgeB, &PeerBridge::peerError, &bridgeB, [&](const QString &m) { peerError = m; });
    bridgeA.start(idA);
    bridgeB.start(idB);
    if (!waitUntil([&] { return openA && openB; }, 60000)) {
        fail(QStringLiteral("PeerJS signalling not reachable or browser engine failed (%1)")
                 .arg(peerError.isEmpty() ? QStringLiteral("no error reported") : peerError));
        return finish();
    }
    pass(QStringLiteral("both peers registered with the PeerJS server"));

    // ---- test payload --------------------------------------------------------------
    const QByteArray big = randomBytes(3 * 1024 * 1024 + 17);
    const QByteArray small = "hello from the self-test\n";
    const QStringList names{QStringLiteral("payload.bin"), QStringLiteral("Album/inner/note.txt")};
    const QStringList paths{writeFile(src.path(), names.at(0), big), writeFile(src.path(), names.at(1), small)};
    const QList<transfer::FileSpec> specs{{names.at(0), big.size()}, {names.at(1), small.size()}};
    QJsonArray filesJson;
    for (const auto &f : specs)
        filesJson.append(QJsonObject{{QStringLiteral("name"), f.name}, {QStringLiteral("size"), static_cast<double>(f.size)}});

    // ---- 3. request / accept through the Worker ----------------------------------------
    QString reqId = QStringLiteral("selftest");
    QString targetPeer = idB;
    if (useWorker) {
        ApiResult r = call([&](CloudflareClient::Callback cb) { clientA.registerSession(idA, cb); });
        ApiResult r2 = call([&](CloudflareClient::Callback cb) { clientB.registerSession(idB, cb); });
        if (!r.ok() || !r2.ok()) {
            fail(QStringLiteral("registering sessions: %1 / %2").arg(r.error, r2.error));
            return finish();
        }
        pass(QStringLiteral("sessions registered"));

        bool socketA = false, socketB = false;
        QJsonObject requestSeen, responseSeen;
        QObject::connect(&clientA, &CloudflareClient::socketStateChanged, &clientA, [&](bool up) { socketA = socketA || up; });
        QObject::connect(&clientB, &CloudflareClient::socketStateChanged, &clientB, [&](bool up) { socketB = socketB || up; });
        QObject::connect(&clientB, &CloudflareClient::notification, &clientB, [&](const QJsonObject &m) {
            if (m.value(QStringLiteral("type")).toString() == QLatin1String("request"))
                requestSeen = m.value(QStringLiteral("request")).toObject();
        });
        QObject::connect(&clientA, &CloudflareClient::notification, &clientA, [&](const QJsonObject &m) {
            if (m.value(QStringLiteral("type")).toString() == QLatin1String("response"))
                responseSeen = m;
        });
        clientA.openSocket();
        clientB.openSocket();
        if (!waitUntil([&] { return socketA && socketB; }, 20000)) {
            fail(QStringLiteral("notification WebSockets did not connect"));
            return finish();
        }
        pass(QStringLiteral("notification WebSockets connected"));

        r = call([&](CloudflareClient::Callback cb) { clientA.lookup(codeB, cb); });
        if (!r.ok() || !r.json.value(QStringLiteral("exists")).toBool() || !r.json.value(QStringLiteral("online")).toBool()) {
            fail(QStringLiteral("lookup of %1 should say exists and online (got status %2 %3)").arg(codeB).arg(r.status).arg(r.error));
            return finish();
        }
        pass(QStringLiteral("lookup reports %1 as existing and online").arg(codeB));

        r = call([&](CloudflareClient::Callback cb) { clientA.sendRequest(codeB, filesJson, cb); });
        reqId = r.json.value(QStringLiteral("id")).toString();
        if (!r.ok() || reqId.isEmpty()) {
            fail(QStringLiteral("sending the request: status %1 %2").arg(r.status).arg(r.error));
            return finish();
        }
        if (!waitUntil([&] { return !requestSeen.isEmpty(); }, 20000)) {
            fail(QStringLiteral("receiver was not notified of the request"));
            return finish();
        }
        if (requestSeen.value(QStringLiteral("from")).toString() != codeA
            || requestSeen.value(QStringLiteral("fromPeerId")).toString() != idA
            || requestSeen.value(QStringLiteral("files")).toArray().size() != 2) {
            fail(QStringLiteral("the notified request has wrong contents"));
            return finish();
        }
        pass(QStringLiteral("receiver was notified of the request over the WebSocket"));

        // Same order as the app: be ready for the connection before replying.
        engineB.expectIncoming(reqId, codeA, idA, specs);
        r = call([&](CloudflareClient::Callback cb) { clientB.respond(reqId, true, cb); });
        if (!r.ok()) {
            fail(QStringLiteral("accepting the request: status %1 %2").arg(r.status).arg(r.error));
            return finish();
        }
        if (!waitUntil([&] { return !responseSeen.isEmpty(); }, 20000)) {
            fail(QStringLiteral("sender was not notified of the answer"));
            return finish();
        }
        targetPeer = responseSeen.value(QStringLiteral("peerId")).toString();
        if (!responseSeen.value(QStringLiteral("accepted")).toBool() || targetPeer != idB) {
            fail(QStringLiteral("the answer does not carry the receiver's peer id"));
            return finish();
        }
        pass(QStringLiteral("sender was told the request was accepted"));
    } else {
        engineB.expectIncoming(reqId, codeA, idA, specs);
    }

    // ---- 4. the actual transfer over WebRTC -----------------------------------------------
    bool senderDone = false, senderOk = false;
    QObject::connect(&engineA, &TransferEngine::sessionFinished, &engineA, [&](bool sending, bool allOk) {
        if (sending) {
            senderDone = true;
            senderOk = allOk;
        }
    });
    QElapsedTimer clock;
    clock.start();
    engineA.startSend(reqId, targetPeer, codeB, paths, names);
    if (!waitUntil([&] { return senderDone; }, 120000)) {
        fail(QStringLiteral("transfer timed out after 120 s"));
        return finish();
    }
    if (!senderOk) {
        QString why;
        for (int i = 0; i < modelA.rowCount(); ++i)
            why += modelA.data(modelA.index(i), TransferModel::ErrorRole).toString() + QLatin1Char(' ');
        fail(QStringLiteral("sender reported a failed transfer: %1").arg(why.trimmed()));
        return finish();
    }
    const qint64 ms = qMax<qint64>(1, clock.elapsed());
    pass(QStringLiteral("transfer completed in %1 ms (%2 KiB/s)").arg(ms).arg((big.size() + small.size()) * 1000 / ms / 1024));

    // ---- 5. verify what arrived -----------------------------------------------------------------
    waitUntil([&] { return modelB.activeCount() == 0 && modelB.rowCount() == 2; }, 10000);
    bool allDone = modelB.rowCount() == 2;
    for (int i = 0; i < modelB.rowCount(); ++i)
        allDone = allDone && modelB.data(modelB.index(i), TransferModel::StateRole).toString() == QLatin1String("Done");
    if (!allDone)
        fail(QStringLiteral("receiver does not report both files as Done"));
    else
        pass(QStringLiteral("receiver reports both files as Done"));

    const QDir d(dst.path());
    if (readFile(d.filePath(names.at(0))) == big && readFile(d.filePath(names.at(1))) == small)
        pass(QStringLiteral("received files are byte-identical, folder structure kept"));
    else
        fail(QStringLiteral("received file contents differ from what was sent"));

    return finish();
}

} // namespace p2p::app
