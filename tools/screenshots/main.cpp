// Usage: p2p_screenshots <scene> <Main.qml> <out.png>
// Scenes: ready | staged | transfer | requests
// Uses the real list models and UI, but a mock controller filled with demo data.
// Run with QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software.
#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include "models/RequestsModel.h"
#include "models/StagingModel.h"
#include "models/TransferModel.h"

using namespace p2p::models;

class MockApp : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString localCode MEMBER m_code CONSTANT)
    Q_PROPERTY(QString status MEMBER m_status CONSTANT)
    Q_PROPERTY(bool online MEMBER m_online CONSTANT)
    Q_PROPERTY(bool busy MEMBER m_busy CONSTANT)
    Q_PROPERTY(QString downloadDir MEMBER m_dir CONSTANT)
    Q_PROPERTY(QObject *staging MEMBER m_staging CONSTANT)
    Q_PROPERTY(QObject *requests MEMBER m_requests CONSTANT)
    Q_PROPERTY(QObject *transfers MEMBER m_transfers CONSTANT)
public:
    QString m_code = QStringLiteral("K7M2QX9P");
    QString m_status = QStringLiteral("Ready");
    bool m_online = true;
    bool m_busy = false;
    QString m_dir = QStringLiteral("C:/Users/you/Downloads/ModernP2P");
    QObject *m_staging = nullptr;
    QObject *m_requests = nullptr;
    QObject *m_transfers = nullptr;

    Q_INVOKABLE void setDownloadDir(const QUrl &) {}
    Q_INVOKABLE void connectToPeer(const QString &) {}
    Q_INVOKABLE void acceptRequest(const QString &) {}
    Q_INVOKABLE void rejectRequest(const QString &) {}
    Q_INVOKABLE void regenerateCode() {}
    Q_INVOKABLE void cancelTransfer(const QString &) {}
    Q_INVOKABLE void addFiles(const QList<QUrl> &) {}
    Q_INVOKABLE void removeStaged(int) {}
    Q_INVOKABLE void clearStaging() {}
};

static void stage(StagingModel &m, QTemporaryDir &dir, const QList<QPair<QString, qint64>> &files)
{
    QStringList paths;
    for (const auto &[name, size] : files) {
        QFile f(dir.filePath(name));
        f.open(QIODevice::WriteOnly);
        f.resize(size); // sparse placeholder so the model reports the demo size
        paths << f.fileName();
    }
    m.addPaths(paths);
}

int main(int argc, char *argv[])
{
    if (argc < 4)
        return 2;
    const QString scene = argv[1];
    const QString qmlFile = argv[2];
    const QString outFile = argv[3];

    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);
    QFont::insertSubstitution(QStringLiteral("Consolas"), QStringLiteral("DejaVu Sans Mono"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QTemporaryDir tmp;
    StagingModel staging;
    RequestsModel requests;
    TransferModel transfers;
    MockApp mock;
    mock.m_staging = &staging;
    mock.m_requests = &requests;
    mock.m_transfers = &transfers;

    constexpr qint64 MB = 1024 * 1024;
    bool openRequests = false;
    QString peerText;

    if (scene == QLatin1String("staged")) {
        stage(staging, tmp, {{"vacation-photos.zip", 148 * MB}, {"Project-Report.pdf", 2 * MB + 400'000},
                             {"demo-video.mp4", 512 * MB}});
        peerText = QStringLiteral("H4N8TW2B");
    } else if (scene == QLatin1String("transfer")) {
        mock.m_busy = true;
        mock.m_status = QStringLiteral("Receiving from Q3ZP6D5M");
        const QString a = transfers.add("old-backup.bin", 1100 * MB, false, "Q3ZP6D5M");
        transfers.update(a, 300 * MB, TransferModel::State::Failed, 0, "Connection closed");
        const QString b = transfers.add("meeting-notes.txt", 18'000, false, "Q3ZP6D5M");
        transfers.update(b, 18'000, TransferModel::State::Verifying);
        const QString c = transfers.add("Project-Report.pdf", 2 * MB + 400'000, true, "H4N8TW2B");
        transfers.update(c, 2 * MB + 400'000, TransferModel::State::Done);
        const QString d = transfers.add("vacation-photos.zip", 148 * MB, true, "H4N8TW2B");
        transfers.update(d, 92 * MB, TransferModel::State::Transferring, 8.4 * MB);
    } else if (scene == QLatin1String("requests")) {
        stage(staging, tmp, {{"slides.pptx", 12 * MB}});
        requests.add({{"id", "a1"}, {"from", "Q3ZP6D5M"},
                      {"files", QJsonArray{QJsonObject{{"name", "holiday-2026.zip"}, {"size", 820.0 * MB}},
                                           QJsonObject{{"name", "readme.txt"}, {"size", 2048}}}}});
        requests.add({{"id", "b2"}, {"from", "H4N8TW2B"},
                      {"files", QJsonArray{QJsonObject{{"name", "invoice-october.pdf"}, {"size", 310'000}}}}});
        openRequests = true;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &mock);
    engine.load(QUrl::fromLocalFile(qmlFile));
    if (engine.rootObjects().isEmpty())
        return 3;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 4;

    if (!peerText.isEmpty()) {
        if (QObject *field = window->findChild<QObject *>(QStringLiteral("peerCode")))
            field->setProperty("text", peerText);
    }
    if (openRequests) {
        if (QObject *popup = window->findChild<QObject *>(QStringLiteral("requestsPopup")))
            QMetaObject::invokeMethod(popup, "open");
    }

    int rc = 5;
    QTimer::singleShot(1200, &app, [&] {
        rc = window->grabWindow().save(outFile) ? 0 : 6;
        app.quit();
    });
    app.exec();
    return rc;
}

#include "main.moc"
