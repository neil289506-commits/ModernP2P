#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

namespace p2p::network {

/// Asks GitHub whether a newer release than the running version exists.
/// Only offers links that point back to this repository on github.com.
class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent = nullptr) : QObject(parent) {}

    void check(const QString &currentVersion);

signals:
    void updateAvailable(const QString &version, const QUrl &url);

private:
    QNetworkAccessManager m_nam;
};

} // namespace p2p::network
