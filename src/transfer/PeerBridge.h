#pragma once

#include <QJsonObject>
#include <QObject>
#include <QStringList>

class QWebChannel;
class QWebEnginePage;
class QWebEngineProfile;

namespace p2p::transfer {

/// Hosts PeerJS in a hidden Qt WebEngine page and exposes it as a small
/// connection API. Connection ids ("o1" outgoing, "i1" incoming) are chosen
/// on the C++ side for outgoing and by the page for incoming connections.
class PeerBridge : public QObject
{
    Q_OBJECT
public:
    explicit PeerBridge(QObject *parent = nullptr);
    ~PeerBridge() override;

    void start(const QString &peerId);
    void reconnectSignaling();
    void connectTo(const QString &cid, const QString &remotePeerId, const QJsonObject &metadata);
    void send(const QString &cid, const QString &message, int seq);
    void closeConnection(const QString &cid);

public slots: // invoked from JavaScript through QWebChannel
    void jsReady();
    void jsPeerOpen(const QString &id);
    void jsPeerDisconnected();
    void jsPeerError(const QString &message);
    void jsIncoming(const QString &cid, const QString &remotePeerId, const QString &metadataJson);
    void jsConnOpen(const QString &cid);
    void jsConnClosed(const QString &cid);
    void jsConnError(const QString &cid, const QString &message);
    void jsMessage(const QString &cid, const QString &message);
    void jsSent(const QString &cid, int seq);

signals:
    void peerOpened(const QString &peerId);
    void peerDisconnected();
    void peerError(const QString &message);
    void incomingConnection(const QString &cid, const QString &remotePeerId, const QJsonObject &metadata);
    void connectionOpened(const QString &cid);
    void connectionClosed(const QString &cid);
    void connectionError(const QString &cid, const QString &message);
    void messageReceived(const QString &cid, const QString &message);
    void messageSent(const QString &cid, int seq);

private:
    void run(const QString &script);

    QWebEngineProfile *m_profile = nullptr;
    QWebEnginePage *m_page = nullptr;
    QWebChannel *m_channel = nullptr;
    bool m_ready = false;
    QStringList m_pending;
};

} // namespace p2p::transfer
