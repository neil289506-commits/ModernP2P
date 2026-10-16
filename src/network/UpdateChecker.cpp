#include "UpdateChecker.h"

#include "core/Version.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace p2p::network {

namespace {
constexpr auto kLatestRelease = "https://api.github.com/repos/neil289506-commits/ModernP2P/releases/latest";
constexpr auto kRepoPathPrefix = "/neil289506-commits/ModernP2P/";
} // namespace

void UpdateChecker::check(const QString &currentVersion)
{
    QNetworkRequest req{QUrl(QString::fromLatin1(kLatestRelease))};
    req.setTransferTimeout(10'000);
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "ModernP2P");

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, currentVersion] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;   // offline, rate limited, or no release yet: stay silent
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = o.value(QStringLiteral("tag_name")).toString();
        const QUrl url(o.value(QStringLiteral("html_url")).toString());
        const bool trusted = url.scheme() == QLatin1String("https") && url.host() == QLatin1String("github.com")
            && url.path().startsWith(QLatin1String(kRepoPathPrefix));
        if (trusted && core::isNewerVersion(tag, currentVersion))
            emit updateAvailable(tag, url);
    });
}

} // namespace p2p::network
