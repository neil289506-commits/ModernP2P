#pragma once

#include <QStringList>

#include "PeerTransport.h"

class QWebChannel;
class QWebEnginePage;
class QWebEngineProfile;

namespace p2p::transfer {

/// Hosts PeerJS in a hidden Qt WebEngine page and exposes it as a small
/// connection API. Connection ids ("o1" outgoing, "i1" incoming) are chosen
/// on the C++ side for outgoing and by the page for incoming connections.
class PeerBridge : public PeerTransport
{
    Q_OBJECT
public:
    explicit PeerBridge(QObject *parent = nullptr);
    ~PeerBridge() override;

    /// Signalling server override from MODERNP2P_PEERJS ("host:port[/path]", optionally with
    /// http:// or https://). Empty object means the public PeerJS server.
    static QJsonObject signallingFromEnvironment();
    void setSignalling(const QJsonObject &options) { m_signalling = options; }

    void start(const QString &peerId);
    void reconnectSignaling();
    void connectTo(const QString &cid, const QString &remotePeerId, const QJsonObject &metadata) override;
    void send(const QString &cid, const QString &message, int seq) override;
    void closeConnection(const QString &cid) override;

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

private:
    void run(const QString &script);

    QWebEngineProfile *m_profile = nullptr;
    QWebEnginePage *m_page = nullptr;
    QWebChannel *m_channel = nullptr;
    QJsonObject m_signalling;
    bool m_ready = false;
    QStringList m_pending;
};

} // namespace p2p::transfer
