#pragma once

#include <QHash>
#include <QPointer>
#include <QTimer>
#include <functional>

#include "transfer/PeerTransport.h"

/// Two of these, paired, behave like two peers connected over a perfect network
/// (delivery happens on the next event-loop turn). `tamper` can rewrite or drop
/// outgoing messages to simulate corruption.
class LoopbackTransport : public p2p::transfer::PeerTransport
{
    Q_OBJECT
public:
    explicit LoopbackTransport(const QString &peerId, QObject *parent = nullptr)
        : PeerTransport(parent), m_peerId(peerId) {}

    static void pair(LoopbackTransport &a, LoopbackTransport &b)
    {
        a.m_other = &b;
        b.m_other = &a;
    }

    std::function<QString(const QString &)> tamper;   ///< return {} to drop a message
    int messagesSent = 0;

    void connectTo(const QString &cid, const QString &remotePeerId, const QJsonObject &metadata) override
    {
        LoopbackTransport *other = m_other;
        if (!other || other->m_peerId != remotePeerId) {
            QTimer::singleShot(0, this, [this, cid] { emit connectionError(cid, QStringLiteral("peer unavailable")); });
            return;
        }
        const QString remoteCid = QStringLiteral("i") + cid;
        m_remote.insert(cid, remoteCid);
        other->m_remote.insert(remoteCid, cid);
        QTimer::singleShot(0, this, [this, other, cid, remoteCid, metadata] {
            emit other->incomingConnection(remoteCid, m_peerId, metadata);
            emit connectionOpened(cid);
            emit other->connectionOpened(remoteCid);
        });
    }

    void send(const QString &cid, const QString &message, int seq) override
    {
        ++messagesSent;
        const QString remoteCid = m_remote.value(cid);
        QString payload = message;
        if (tamper)
            payload = tamper(message);
        LoopbackTransport *other = m_other;
        if (!remoteCid.isEmpty() && other && !payload.isEmpty()) {
            QTimer::singleShot(0, other, [other, remoteCid, payload] { emit other->messageReceived(remoteCid, payload); });
        }
        QTimer::singleShot(0, this, [this, cid, seq] { emit messageSent(cid, seq); });
    }

    void closeConnection(const QString &cid) override
    {
        const QString remoteCid = m_remote.take(cid);
        if (remoteCid.isEmpty())
            return;
        LoopbackTransport *other = m_other;
        other->m_remote.remove(remoteCid);
        QTimer::singleShot(0, this, [this, cid] { emit connectionClosed(cid); });
        QTimer::singleShot(0, other, [other, remoteCid] { emit other->connectionClosed(remoteCid); });
    }

private:
    QString m_peerId;
    QPointer<LoopbackTransport> m_other;
    QHash<QString, QString> m_remote;
};
