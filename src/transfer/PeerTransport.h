#pragma once

#include <QJsonObject>
#include <QObject>

namespace p2p::transfer {

/// What the transfer engine needs from a peer connection layer. The real
/// implementation is PeerBridge (PeerJS in a WebEngine page); tests use an
/// in-process loopback.
class PeerTransport : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    virtual void connectTo(const QString &cid, const QString &remotePeerId, const QJsonObject &metadata) = 0;
    virtual void send(const QString &cid, const QString &message, int seq) = 0;
    virtual void closeConnection(const QString &cid) = 0;

signals:
    void incomingConnection(const QString &cid, const QString &remotePeerId, const QJsonObject &metadata);
    void connectionOpened(const QString &cid);
    void connectionClosed(const QString &cid);
    void connectionError(const QString &cid, const QString &message);
    void messageReceived(const QString &cid, const QString &message);
    void messageSent(const QString &cid, int seq);
};

} // namespace p2p::transfer
