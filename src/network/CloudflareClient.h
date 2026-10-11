#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>
#include <functional>

namespace p2p::network {

struct ApiResult
{
    int status = 0;        ///< HTTP status, 0 when the request never completed
    QJsonObject json;
    QString error;

    bool ok() const { return status >= 200 && status < 300; }
};

/// Talks to the Cloudflare Worker: code registry, requests and the
/// notification WebSocket.
class CloudflareClient : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(const ApiResult &)>;

    explicit CloudflareClient(QObject *parent = nullptr);

    void setBaseUrl(const QUrl &url);
    bool isConfigured() const { return m_base.isValid() && !m_base.host().isEmpty(); }
    void setCredentials(const QString &code, const QString &token);

    void claim(const QString &code, Callback cb);          ///< result.json["ownerToken"]
    void release(Callback cb);
    void registerSession(const QString &peerId, Callback cb);
    void lookup(const QString &code, Callback cb);         ///< {exists, online}
    void sendRequest(const QString &toCode, const QJsonArray &files, Callback cb);
    void fetchInbox(Callback cb);                          ///< {requests:[...]}
    void respond(const QString &requestId, bool accept, Callback cb);

    void openSocket();
    void closeSocket();

signals:
    void notification(const QJsonObject &message);
    void socketStateChanged(bool connected);

private:
    struct Options
    {
        bool auth = false;
        bool fromCode = false;
    };
    void send(const QByteArray &method, const QString &path, const QJsonObject &body,
              Options opt, Callback cb);
    void connectSocketNow();
    QString codePath(const QString &code, const QString &suffix = {}) const;

    QNetworkAccessManager m_nam;
    QUrl m_base;
    QString m_code;
    QString m_token;

    QWebSocket m_ws;
    QTimer m_reconnect;
    QTimer m_ping;
    bool m_wantSocket = false;
    int m_backoffMs = 1000;
};

} // namespace p2p::network
