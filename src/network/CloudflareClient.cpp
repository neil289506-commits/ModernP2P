#include "CloudflareClient.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace p2p::network {

CloudflareClient::CloudflareClient(QObject *parent)
    : QObject(parent)
{
    m_reconnect.setSingleShot(true);
    connect(&m_reconnect, &QTimer::timeout, this, &CloudflareClient::connectSocketNow);

    m_ping.setInterval(25'000);
    connect(&m_ping, &QTimer::timeout, this, [this] {
        if (m_ws.state() == QAbstractSocket::ConnectedState)
            m_ws.sendTextMessage(QStringLiteral("ping"));
    });

    connect(&m_ws, &QWebSocket::connected, this, [this] {
        m_backoffMs = 1000;
        m_ping.start();
        emit socketStateChanged(true);
    });
    connect(&m_ws, &QWebSocket::disconnected, this, [this] {
        m_ping.stop();
        emit socketStateChanged(false);
        if (m_wantSocket) {
            m_reconnect.start(m_backoffMs);
            m_backoffMs = qMin(m_backoffMs * 2, 30'000);
        }
    });
    connect(&m_ws, &QWebSocket::textMessageReceived, this, [this](const QString &text) {
        const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
        if (doc.isObject())
            emit notification(doc.object());
    });
}

void CloudflareClient::setBaseUrl(const QUrl &url)
{
    m_base = url;
}

void CloudflareClient::setCredentials(const QString &code, const QString &token)
{
    m_code = code;
    m_token = token;
}

QString CloudflareClient::codePath(const QString &code, const QString &suffix) const
{
    QString path = QStringLiteral("/v1/codes/") + code;
    if (!suffix.isEmpty())
        path += QLatin1Char('/') + suffix;
    return path;
}

void CloudflareClient::send(const QByteArray &method, const QString &path, const QJsonObject &body,
                            Options opt, Callback cb)
{
    if (!isConfigured()) {
        ApiResult r;
        r.error = QStringLiteral("Server URL is not configured");
        cb(r);
        return;
    }

    QUrl url = m_base;
    url.setPath(path);
    QNetworkRequest req(url);
    req.setTransferTimeout(15'000);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (opt.auth)
        req.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    if (opt.fromCode)
        req.setRawHeader("X-From-Code", m_code.toUtf8());

    QNetworkReply *reply = nullptr;
    if (method == "GET")
        reply = m_nam.get(req);
    else
        reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [reply, cb = std::move(cb)] {
        ApiResult r;
        r.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject())
            r.json = doc.object();
        if (r.status == 0)
            r.error = reply->errorString();
        else if (!r.ok())
            r.error = r.json.value(QStringLiteral("error")).toString(QStringLiteral("HTTP %1").arg(r.status));
        reply->deleteLater();
        cb(r);
    });
}

void CloudflareClient::claim(const QString &code, Callback cb)
{
    send("POST", codePath(code, QStringLiteral("claim")), {}, {}, std::move(cb));
}

void CloudflareClient::release(Callback cb)
{
    send("POST", codePath(m_code, QStringLiteral("release")), {}, {.auth = true}, std::move(cb));
}

void CloudflareClient::registerSession(const QString &peerId, Callback cb)
{
    send("POST", codePath(m_code, QStringLiteral("session")),
         QJsonObject{{QStringLiteral("peerId"), peerId}}, {.auth = true}, std::move(cb));
}

void CloudflareClient::lookup(const QString &code, Callback cb)
{
    send("GET", codePath(code), {}, {}, std::move(cb));
}

void CloudflareClient::sendRequest(const QString &toCode, const QJsonArray &files, Callback cb)
{
    send("POST", codePath(toCode, QStringLiteral("requests")),
         QJsonObject{{QStringLiteral("files"), files}}, {.auth = true, .fromCode = true}, std::move(cb));
}

void CloudflareClient::fetchInbox(Callback cb)
{
    send("GET", codePath(m_code, QStringLiteral("requests")), {}, {.auth = true}, std::move(cb));
}

void CloudflareClient::respond(const QString &requestId, bool accept, Callback cb)
{
    send("POST", codePath(m_code, QStringLiteral("requests/%1/respond").arg(requestId)),
         QJsonObject{{QStringLiteral("accept"), accept}}, {.auth = true}, std::move(cb));
}

void CloudflareClient::openSocket()
{
    m_wantSocket = true;
    connectSocketNow();
}

void CloudflareClient::closeSocket()
{
    m_wantSocket = false;
    m_reconnect.stop();
    m_ws.close();
}

void CloudflareClient::connectSocketNow()
{
    if (!m_wantSocket || !isConfigured() || m_token.isEmpty())
        return;
    if (m_ws.state() != QAbstractSocket::UnconnectedState)
        return;

    QUrl url = m_base;
    url.setScheme(url.scheme() == QLatin1String("http") ? QStringLiteral("ws") : QStringLiteral("wss"));
    url.setPath(codePath(m_code, QStringLiteral("ws")));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    m_ws.open(req);
}

} // namespace p2p::network
