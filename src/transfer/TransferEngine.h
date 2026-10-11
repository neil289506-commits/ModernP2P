#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <memory>

#include "models/TransferModel.h"

namespace p2p::transfer {

class PeerBridge;

struct FileSpec
{
    QString name;
    qint64 size = 0;
};

/// Runs the file transfer protocol over PeerJS connections.
///
/// Messages are JSON strings:
///   sender   -> {"t":"file","name","size"}, {"t":"chunk","d":<base64>}, {"t":"end","sha":<hex>}
///   receiver -> {"t":"ack","ok":true|false,"reason"}
/// Files are sent one after another; each is verified with SHA-256 before the
/// receiver renames it from "<name>.part" to its final name.
class TransferEngine : public QObject
{
    Q_OBJECT
public:
    static constexpr int kChunkSize = 48 * 1024;
    static constexpr int kWindow = 4;                 ///< chunks in flight to the page
    static constexpr int kConnectTimeoutMs = 30'000;

    TransferEngine(PeerBridge *bridge, models::TransferModel *model, QObject *parent = nullptr);

    void setDownloadDir(const QString &dir) { m_downloadDir = dir; }
    QString downloadDir() const { return m_downloadDir; }

    /// The user accepted `reqId`; only connections presenting it (with exactly
    /// these files) are accepted, once.
    void expectIncoming(const QString &reqId, const QString &fromCode, const QString &fromPeerId,
                        const QList<FileSpec> &files);
    void startSend(const QString &reqId, const QString &peerId, const QString &toCode,
                   const QStringList &paths);
    void cancel(const QString &transferId);

signals:
    void statusMessage(const QString &text);
    void transferStarted(bool sending);
    void sessionFinished(bool sending, bool allOk);

private:
    struct OutSession
    {
        QString cid, reqId, peerCode;
        QStringList paths, tids;
        int idx = -1;
        bool opened = false, eof = false, endSent = false, finished = false;
        std::unique_ptr<QFile> file;
        QCryptographicHash hash{QCryptographicHash::Sha256};
        int outstanding = 0, seq = 0;
        qint64 sent = 0;
        QElapsedTimer clock, progress;
        bool allOk = true;
    };
    struct InSession
    {
        QString cid, reqId, fromCode;
        QList<FileSpec> files;
        QStringList tids;
        int idx = -1;
        std::unique_ptr<QFile> file;
        QCryptographicHash hash{QCryptographicHash::Sha256};
        qint64 received = 0;
        QString partPath, finalPath;
        QElapsedTimer clock, progress;
        bool allOk = true;
    };
    struct Expected { QString fromCode, fromPeerId; QList<FileSpec> files; };

    void onIncoming(const QString &cid, const QString &peer, const QJsonObject &meta);
    void onOpened(const QString &cid);
    void onClosed(const QString &cid);
    void onError(const QString &cid, const QString &message);
    void onMessage(const QString &cid, const QString &message);
    void onSent(const QString &cid, int seq);

    void nextFile(OutSession &s);
    void pump(OutSession &s);
    void post(OutSession &s, const QJsonObject &obj);
    void failOut(OutSession &s, const QString &reason, models::TransferModel::State st = models::TransferModel::State::Failed);
    void handleOut(OutSession &s, const QJsonObject &o);

    void handleIn(InSession &s, const QJsonObject &o);
    void failIn(InSession &s, const QString &reason, models::TransferModel::State st = models::TransferModel::State::Failed);
    void reply(const InSession &s, bool ok, const QString &reason = {});

    void report(const QString &tid, qint64 done, models::TransferModel::State state, QElapsedTimer &clock,
                QElapsedTimer &throttle, bool force);

    PeerBridge *m_bridge;
    models::TransferModel *m_model;
    QString m_downloadDir;
    int m_counter = 0;

    QHash<QString, Expected> m_expected;                       // reqId -> accepted request
    QHash<QString, std::shared_ptr<OutSession>> m_out;         // cid -> session
    QHash<QString, std::shared_ptr<InSession>> m_in;
    QHash<QString, QString> m_tidToCid;
};

} // namespace p2p::transfer
