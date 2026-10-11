#include "TransferEngine.h"

#include "PeerBridge.h"
#include "core/FileNames.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTimer>

namespace p2p::transfer {

using models::TransferModel;
using State = TransferModel::State;

namespace {

QString toJson(const QJsonObject &o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString hex(QCryptographicHash &h)
{
    return QString::fromLatin1(h.result().toHex());
}

constexpr int kExpectedTtlMs = 10 * 60 * 1000;

} // namespace

TransferEngine::TransferEngine(PeerBridge *bridge, TransferModel *model, QObject *parent)
    : QObject(parent)
    , m_bridge(bridge)
    , m_model(model)
{
    connect(bridge, &PeerBridge::incomingConnection, this, &TransferEngine::onIncoming);
    connect(bridge, &PeerBridge::connectionOpened, this, &TransferEngine::onOpened);
    connect(bridge, &PeerBridge::connectionClosed, this, &TransferEngine::onClosed);
    connect(bridge, &PeerBridge::connectionError, this, &TransferEngine::onError);
    connect(bridge, &PeerBridge::messageReceived, this, &TransferEngine::onMessage);
    connect(bridge, &PeerBridge::messageSent, this, &TransferEngine::onSent);
}

void TransferEngine::report(const QString &tid, qint64 done, State state, QElapsedTimer &clock,
                            QElapsedTimer &throttle, bool force)
{
    if (!force && throttle.isValid() && throttle.elapsed() < 100)
        return;
    throttle.restart();
    const double secs = clock.isValid() ? clock.elapsed() / 1000.0 : 0.0;
    m_model->update(tid, done, state, secs > 0 ? done / secs : 0.0);
}

// ---------------------------------------------------------------- sending --

void TransferEngine::startSend(const QString &reqId, const QString &peerId, const QString &toCode,
                               const QStringList &paths)
{
    auto s = std::make_shared<OutSession>();
    s->cid = QStringLiteral("o%1").arg(++m_counter);
    s->reqId = reqId;
    s->peerCode = toCode;
    s->paths = paths;
    for (const QString &path : paths) {
        const QFileInfo fi(path);
        const QString tid = m_model->add(fi.fileName(), fi.size(), true, toCode);
        s->tids << tid;
        m_tidToCid.insert(tid, s->cid);
    }
    m_out.insert(s->cid, s);
    m_bridge->connectTo(s->cid, peerId, QJsonObject{{QStringLiteral("reqId"), reqId}});

    const QString cid = s->cid;
    QTimer::singleShot(kConnectTimeoutMs, this, [this, cid] {
        const auto it = m_out.find(cid);
        if (it != m_out.end() && !(*it)->opened)
            failOut(**it, QStringLiteral("Connection timed out"));
    });
}

void TransferEngine::nextFile(OutSession &s)
{
    s.file.reset();
    ++s.idx;
    if (s.idx >= s.paths.size()) {
        s.finished = true;
        emit statusMessage(tr("Sent %n file(s) to %1", nullptr, static_cast<int>(s.paths.size())).arg(s.peerCode));
        emit sessionFinished(true, s.allOk);
        m_bridge->closeConnection(s.cid);
        return;
    }

    s.file = std::make_unique<QFile>(s.paths.at(s.idx));
    if (!s.file->open(QIODevice::ReadOnly)) {
        failOut(s, QStringLiteral("Cannot read file"));
        return;
    }
    s.hash.reset();
    s.sent = 0;
    s.eof = false;
    s.endSent = false;
    s.clock.start();
    s.progress.start();

    post(s, QJsonObject{{QStringLiteral("t"), QStringLiteral("file")},
                        {QStringLiteral("name"), QFileInfo(s.paths.at(s.idx)).fileName()},
                        {QStringLiteral("size"), static_cast<double>(s.file->size())}});
    m_model->update(s.tids.at(s.idx), 0, State::Transferring);
    pump(s);
}

void TransferEngine::post(OutSession &s, const QJsonObject &obj)
{
    ++s.outstanding;
    m_bridge->send(s.cid, toJson(obj), ++s.seq);
}

void TransferEngine::pump(OutSession &s)
{
    if (s.finished || !s.file)
        return;

    while (s.outstanding < kWindow && !s.eof) {
        const QByteArray buf = s.file->read(kChunkSize);
        if (buf.isEmpty()) {
            s.eof = true;
            break;
        }
        s.hash.addData(buf);
        s.sent += buf.size();
        post(s, QJsonObject{{QStringLiteral("t"), QStringLiteral("chunk")},
                            {QStringLiteral("d"), QString::fromLatin1(buf.toBase64())}});
    }

    const QString &tid = s.tids.at(s.idx);
    if (s.eof && !s.endSent) {
        s.endSent = true;
        post(s, QJsonObject{{QStringLiteral("t"), QStringLiteral("end")}, {QStringLiteral("sha"), hex(s.hash)}});
        report(tid, s.sent, State::Verifying, s.clock, s.progress, true);
    } else if (!s.endSent) {
        report(tid, s.sent, State::Transferring, s.clock, s.progress, false);
    }
}

void TransferEngine::onSent(const QString &cid, int)
{
    const auto it = m_out.find(cid);
    if (it == m_out.end())
        return;
    OutSession &s = **it;
    if (s.outstanding > 0)
        --s.outstanding;
    pump(s);
}

void TransferEngine::failOut(OutSession &s, const QString &reason, State st)
{
    if (s.finished)
        return;
    s.finished = true;
    s.allOk = false;
    s.file.reset();
    for (int i = qMax(s.idx, 0); i < s.tids.size(); ++i) {
        if (m_model->isActive(s.tids.at(i)))
            m_model->update(s.tids.at(i), i == s.idx ? s.sent : 0, st, 0, reason);
    }
    emit statusMessage(st == State::Cancelled ? tr("Transfer cancelled") : tr("Transfer failed: %1").arg(reason));
    emit sessionFinished(true, false);
    m_bridge->closeConnection(s.cid);
}

void TransferEngine::handleOut(OutSession &s, const QJsonObject &o)
{
    if (o.value(QStringLiteral("t")).toString() != QLatin1String("ack") || s.idx < 0 || s.idx >= s.tids.size())
        return;
    if (o.value(QStringLiteral("ok")).toBool()) {
        m_model->update(s.tids.at(s.idx), s.sent, State::Done);
        nextFile(s);
    } else {
        failOut(s, o.value(QStringLiteral("reason")).toString(QStringLiteral("Rejected by receiver")));
    }
}

// -------------------------------------------------------------- receiving --

void TransferEngine::expectIncoming(const QString &reqId, const QString &fromCode, const QString &fromPeerId,
                                    const QList<FileSpec> &files)
{
    m_expected.insert(reqId, {fromCode, fromPeerId, files});
    QTimer::singleShot(kExpectedTtlMs, this, [this, reqId] { m_expected.remove(reqId); });
}

void TransferEngine::onIncoming(const QString &cid, const QString &peer, const QJsonObject &meta)
{
    const QString reqId = meta.value(QStringLiteral("reqId")).toString();
    const auto it = m_expected.find(reqId);
    // Only the peer the Worker vouched for, with an accepted request id, may connect.
    if (it == m_expected.end() || it->fromPeerId != peer) {
        m_bridge->closeConnection(cid);
        return;
    }

    auto s = std::make_shared<InSession>();
    s->cid = cid;
    s->reqId = reqId;
    s->fromCode = it->fromCode;
    s->files = it->files;
    m_expected.erase(it);

    QDir().mkpath(m_downloadDir);
    for (const FileSpec &f : std::as_const(s->files)) {
        const QString tid = m_model->add(f.name, f.size, false, s->fromCode);
        s->tids << tid;
        m_tidToCid.insert(tid, cid);
    }
    m_in.insert(cid, s);
    emit statusMessage(tr("Receiving from %1").arg(s->fromCode));
    emit transferStarted(false);
}

void TransferEngine::handleIn(InSession &s, const QJsonObject &o)
{
    const QString t = o.value(QStringLiteral("t")).toString();

    if (t == QLatin1String("file")) {
        if (s.file || s.idx + 1 >= s.files.size())
            return failIn(s, QStringLiteral("Protocol error"));
        ++s.idx;
        const FileSpec &spec = s.files.at(s.idx);
        if (static_cast<qint64>(o.value(QStringLiteral("size")).toDouble(-1)) != spec.size)
            return failIn(s, QStringLiteral("File size does not match the request"));

        // The name comes from the request the user accepted, never from the wire.
        s.finalPath = core::uniqueFilePath(QDir(m_downloadDir), core::sanitizeFileName(spec.name));
        s.partPath = s.finalPath + QStringLiteral(".part");
        s.file = std::make_unique<QFile>(s.partPath);
        if (!s.file->open(QIODevice::WriteOnly | QIODevice::Truncate))
            return failIn(s, QStringLiteral("Cannot write to the download folder"));
        s.hash.reset();
        s.received = 0;
        s.clock.start();
        s.progress.start();
        m_model->update(s.tids.at(s.idx), 0, State::Transferring);
        return;
    }

    if (t == QLatin1String("chunk")) {
        if (!s.file)
            return failIn(s, QStringLiteral("Protocol error"));
        const QByteArray data = QByteArray::fromBase64(o.value(QStringLiteral("d")).toString().toLatin1());
        if (s.received + data.size() > s.files.at(s.idx).size)
            return failIn(s, QStringLiteral("More data than announced"));
        if (s.file->write(data) != data.size())
            return failIn(s, QStringLiteral("Disk write failed"));
        s.hash.addData(data);
        s.received += data.size();
        report(s.tids.at(s.idx), s.received, State::Transferring, s.clock, s.progress, false);
        return;
    }

    if (t == QLatin1String("end")) {
        if (!s.file)
            return failIn(s, QStringLiteral("Protocol error"));
        s.file->close();
        s.file.reset();
        const QString tid = s.tids.at(s.idx);
        QString problem;
        if (s.received != s.files.at(s.idx).size)
            problem = QStringLiteral("Incomplete file");
        else if (hex(s.hash) != o.value(QStringLiteral("sha")).toString().toLower())
            problem = QStringLiteral("Checksum mismatch");
        else if (!QFile::rename(s.partPath, s.finalPath))
            problem = QStringLiteral("Cannot save file");

        if (problem.isEmpty()) {
            m_model->update(tid, s.received, State::Done);
            reply(s, true);
        } else {
            QFile::remove(s.partPath);
            s.allOk = false;
            m_model->update(tid, s.received, State::Failed, 0, problem);
            reply(s, false, problem);
        }
        if (s.idx + 1 == s.files.size()) {
            emit statusMessage(s.allOk ? tr("Received %n file(s) from %1", nullptr, static_cast<int>(s.files.size())).arg(s.fromCode)
                                       : tr("Some files from %1 failed").arg(s.fromCode));
            emit sessionFinished(false, s.allOk);
        }
    }
}

void TransferEngine::reply(const InSession &s, bool ok, const QString &reason)
{
    m_bridge->send(s.cid,
                   toJson({{QStringLiteral("t"), QStringLiteral("ack")}, {QStringLiteral("ok"), ok},
                           {QStringLiteral("reason"), reason}}),
                   0);
}

void TransferEngine::failIn(InSession &s, const QString &reason, State st)
{
    if (s.file) {
        s.file->close();
        s.file.reset();
        QFile::remove(s.partPath);
    }
    s.allOk = false;
    for (int i = qMax(s.idx, 0); i < s.tids.size(); ++i) {
        if (m_model->isActive(s.tids.at(i)))
            m_model->update(s.tids.at(i), 0, st, 0, reason);
    }
    emit statusMessage(st == State::Cancelled ? tr("Transfer cancelled") : tr("Transfer failed: %1").arg(reason));
    emit sessionFinished(false, false);
    m_bridge->closeConnection(s.cid);
}

// ----------------------------------------------------------------- shared --

void TransferEngine::onOpened(const QString &cid)
{
    const auto it = m_out.find(cid);
    if (it == m_out.end())
        return;
    (*it)->opened = true;
    emit transferStarted(true);
    nextFile(**it);
}

void TransferEngine::onMessage(const QString &cid, const QString &message)
{
    const QJsonObject o = QJsonDocument::fromJson(message.toUtf8()).object();
    if (const auto out = m_out.find(cid); out != m_out.end())
        handleOut(**out, o);
    else if (const auto in = m_in.find(cid); in != m_in.end())
        handleIn(**in, o);
}

void TransferEngine::onClosed(const QString &cid)
{
    if (const auto out = m_out.find(cid); out != m_out.end()) {
        failOut(**out, QStringLiteral("Connection closed"));
        m_out.erase(out);
    } else if (const auto in = m_in.find(cid); in != m_in.end()) {
        InSession &s = **in;
        bool pending = false;
        for (const QString &tid : std::as_const(s.tids))
            pending = pending || m_model->isActive(tid);
        if (pending)
            failIn(s, QStringLiteral("Connection closed"));
        m_in.erase(in);
    }
}

void TransferEngine::onError(const QString &cid, const QString &message)
{
    if (const auto out = m_out.find(cid); out != m_out.end())
        failOut(**out, message);
    else if (const auto in = m_in.find(cid); in != m_in.end())
        failIn(**in, message);
}

void TransferEngine::cancel(const QString &transferId)
{
    const QString cid = m_tidToCid.value(transferId);
    if (const auto out = m_out.find(cid); out != m_out.end())
        failOut(**out, QStringLiteral("Cancelled"), State::Cancelled);
    else if (const auto in = m_in.find(cid); in != m_in.end())
        failIn(**in, QStringLiteral("Cancelled"), State::Cancelled);
}

} // namespace p2p::transfer
