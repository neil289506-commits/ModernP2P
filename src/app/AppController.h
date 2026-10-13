#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QUrl>

#include "core/Identity.h"
#include "models/RequestsModel.h"
#include "models/StagingModel.h"
#include "models/TransferModel.h"
#include "network/CloudflareClient.h"
#include "transfer/PeerBridge.h"
#include "transfer/TransferEngine.h"

namespace p2p::app {

/// Glue between the QML UI, the Cloudflare Worker and the PeerJS bridge.
class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString localCode READ localCode NOTIFY localCodeChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString downloadDir READ downloadDir NOTIFY downloadDirChanged)
    Q_PROPERTY(QObject *staging READ staging CONSTANT)
    Q_PROPERTY(QObject *requests READ requests CONSTANT)
    Q_PROPERTY(QObject *transfers READ transfers CONSTANT)
public:
    explicit AppController(QObject *parent = nullptr);

    QString localCode() const { return m_identity.code; }
    QString status() const { return m_status; }
    bool online() const { return m_online; }
    bool busy() const { return m_busy; }
    QString downloadDir() const { return m_engine.downloadDir(); }
    QObject *staging() { return &m_staging; }
    QObject *requests() { return &m_requests; }
    QObject *transfers() { return &m_transfers; }

    /// Call once after the event loop is running.
    Q_INVOKABLE void start();
    Q_INVOKABLE void connectToPeer(const QString &rawCode);
    Q_INVOKABLE void acceptRequest(const QString &requestId);
    Q_INVOKABLE void rejectRequest(const QString &requestId);
    Q_INVOKABLE void regenerateCode();
    Q_INVOKABLE void cancelTransfer(const QString &transferId);
    Q_INVOKABLE void addFiles(const QList<QUrl> &urls);
    Q_INVOKABLE void removeStaged(int row) { m_staging.removeAt(row); }
    Q_INVOKABLE void clearStaging() { m_staging.clear(); }
    Q_INVOKABLE void setDownloadDir(const QUrl &folder);

signals:
    void localCodeChanged();
    void statusChanged();
    void onlineChanged();
    void busyChanged();
    void downloadDirChanged();
    void requestArrived();

private:
    struct Outgoing { QStringList paths; QString toCode; };

    void setStatus(const QString &text);
    void setOnline(bool online);
    void setBusy(bool busy);
    void claimNewCode(int attempt);
    void useIdentity(const core::Identity &id);
    void startSession();
    void refreshInbox();
    void onNotification(const QJsonObject &msg);

    network::CloudflareClient m_client;
    transfer::PeerBridge m_bridge;
    models::StagingModel m_staging;
    models::RequestsModel m_requests;
    models::TransferModel m_transfers;
    transfer::TransferEngine m_engine;

    core::Identity m_identity;
    QString m_peerId;
    QString m_status;
    bool m_online = false;
    bool m_busy = false;
    bool m_peerOpen = false;
    bool m_socketUp = false;
    QHash<QString, Outgoing> m_outgoing;   // request id -> what to send once accepted
};

} // namespace p2p::app
