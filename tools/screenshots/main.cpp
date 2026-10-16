// Usage: p2p_screenshots <scene> <Main.qml> <out.png> [dark] [translations.qm]
// Scenes: ready | update | staged | transfer | requests | history | blocked
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

#include <QTranslator>

#include "models/BlockListModel.h"
#include "models/HistoryModel.h"
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
    Q_PROPERTY(QObject *history MEMBER m_history CONSTANT)
    Q_PROPERTY(QObject *blocked MEMBER m_blocked CONSTANT)
    Q_PROPERTY(QString version MEMBER m_version CONSTANT)
    Q_PROPERTY(QString updateVersion MEMBER m_updateVersion CONSTANT)
public:
    QString m_code = QStringLiteral("K7M2QX9P");
    QString m_status = QStringLiteral("Ready");
    bool m_online = true;
    bool m_busy = false;
    QString m_dir = QStringLiteral("C:/Users/you/Downloads/ModernP2P");
    QObject *m_staging = nullptr;
    QObject *m_requests = nullptr;
    QObject *m_transfers = nullptr;
    QObject *m_history = nullptr;
    QObject *m_blocked = nullptr;
    QString m_version = QStringLiteral("0.1.0");
    QString m_updateVersion;

signals:
    void requestArrived();

public:
    Q_INVOKABLE void setDownloadDir(const QUrl &) {}
    Q_INVOKABLE void connectToPeer(const QString &) {}
    Q_INVOKABLE void acceptRequest(const QString &) {}
    Q_INVOKABLE void rejectRequest(const QString &) {}
    Q_INVOKABLE void regenerateCode() {}
    Q_INVOKABLE void cancelTransfer(const QString &) {}
    Q_INVOKABLE void addFiles(const QList<QUrl> &) {}
    Q_INVOKABLE void removeStaged(int) {}
    Q_INVOKABLE void clearStaging() {}
    Q_INVOKABLE void blockRequest(const QString &) {}
    Q_INVOKABLE void unblock(const QString &) {}
    Q_INVOKABLE void clearHistory() {}
    Q_INVOKABLE void showInFolder(const QString &) {}
    Q_INVOKABLE void openLogFolder() {}
    Q_INVOKABLE void openUpdatePage() {}
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
    bool dark = false;
    QString qmFile;
    for (int i = 4; i < argc; ++i) {
        const QString a = argv[i];
        if (a == QLatin1String("dark"))
            dark = true;
        else if (a.endsWith(QLatin1String(".qm")))
            qmFile = a;
    }

    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);
    QFont::insertSubstitution(QStringLiteral("Consolas"), QStringLiteral("DejaVu Sans Mono"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QTranslator translator;
    if (!qmFile.isEmpty() && translator.load(qmFile))
        app.installTranslator(&translator);

    QTemporaryDir tmp;
    StagingModel staging;
    RequestsModel requests;
    TransferModel transfers;
    HistoryModel history(tmp.filePath("history.json"));
    BlockListModel blocked(tmp.filePath("blocked.json"));
    MockApp mock;
    mock.m_history = &history;
    mock.m_blocked = &blocked;
    mock.m_staging = &staging;
    mock.m_requests = &requests;
    mock.m_transfers = &transfers;

    constexpr qint64 MB = 1024 * 1024;
    bool openRequests = false;
    QString peerText;
    QString openPopup;
    int popupTab = 0;

    if (scene == QLatin1String("update")) {
        mock.m_updateVersion = QStringLiteral("v0.2.0");
    } else if (scene == QLatin1String("staged")) {
        stage(staging, tmp, {{"vacation-photos.zip", 148 * MB}, {"Project-Report.pdf", 2 * MB + 400'000},
                             {"demo-video.mp4", 512 * MB}});
        peerText = QStringLiteral("H4N8TW2B");
    } else if (scene == QLatin1String("transfer")) {
        mock.m_busy = true;
        mock.m_status = QStringLiteral("Receiving from Q3ZP6D5M");
        const QString a = transfers.add("old-backup.bin", 1100 * MB, false, "Q3ZP6D5M");
        transfers.update(a, 300 * MB, TransferModel::State::Failed, 0, "Connection closed");
        const QString b = transfers.add("holiday/IMG_2041.jpg", 3 * MB, false, "Q3ZP6D5M");
        transfers.setPath(b, QStringLiteral("C:/Users/you/Downloads/ModernP2P/holiday/IMG_2041.jpg"));
        transfers.update(b, 3 * MB, TransferModel::State::Done);
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
    } else if (scene == QLatin1String("history") || scene == QLatin1String("blocked")) {
        const auto now = QDateTime::currentDateTime();
        auto add = [&](const char *name, const char *peer, bool sending, const char *result, qint64 size, int minutesAgo,
                       const char *path = "") {
            HistoryModel::Entry e;
            e.name = QString::fromUtf8(name); e.peer = peer; e.sending = sending; e.result = result;
            e.size = size; e.when = now.addSecs(-60 * minutesAgo); e.path = path;
            history.add(e);
        };
        add("vacation-photos.zip", "H4N8TW2B", true, "Done", 148 * MB, 190);
        add("old-backup.bin", "Q3ZP6D5M", false, "Failed", 1100 * MB, 95);
        add("holiday/IMG_2041.jpg", "Q3ZP6D5M", false, "Done", 3 * MB, 42, "C:/x/holiday/IMG_2041.jpg");
        add("Project-Report.pdf", "H4N8TW2B", true, "Done", 2 * MB + 400'000, 12);
        add("draft.docx", "Q3ZP6D5M", true, "Cancelled", 640'000, 3);
        blocked.add(QStringLiteral("X9K2M4PQ"));
        blocked.add(QStringLiteral("7TRW5ABC"));
        openPopup = QStringLiteral("historyPopup");
        popupTab = scene == QLatin1String("blocked") ? 1 : 0;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &mock);
    engine.rootContext()->setContextProperty(QStringLiteral("forceDark"), dark);
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
    if (!openPopup.isEmpty()) {
        if (QObject *popup = window->findChild<QObject *>(openPopup)) {
            popup->setProperty("tab", popupTab);
            QMetaObject::invokeMethod(popup, "open");
        }
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
