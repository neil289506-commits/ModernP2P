#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QTranslator>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QRegularExpression>
#include <QTimer>

#include "app/AppController.h"
#include "app/SelfTest.h"
#include "core/Logger.h"

int main(int argc, char *argv[])
{
    // Background pages must not be throttled or the PeerJS host would stall.
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
            "--disable-background-timer-throttling --disable-renderer-backgrounding");
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QGuiApplication app(argc, argv);

    // Command line: --profile <name> runs an independent copy (own code, settings,
    // history and download folder) so two instances can talk to each other on one PC.
    // --selftest [--selftest-out=<file>] runs the automated end-to-end check and exits.
    QString profile = qEnvironmentVariable("MODERNP2P_PROFILE");
    QString selftestOut;
    bool selftest = false;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i) == QLatin1String("--profile") && i + 1 < args.size())
            profile = args.at(++i);
        else if (args.at(i).startsWith(QLatin1String("--profile=")))
            profile = args.at(i).mid(10);
        else if (args.at(i) == QLatin1String("--selftest"))
            selftest = true;
        else if (args.at(i).startsWith(QLatin1String("--selftest-out=")))
            selftestOut = args.at(i).mid(15);
    }
    if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9_-]{1,16}$")).match(profile).hasMatch())
        profile.clear();

    QGuiApplication::setApplicationName(profile.isEmpty() ? QStringLiteral("ModernP2P")
                                                           : QStringLiteral("ModernP2P-") + profile);
    QGuiApplication::setOrganizationName(QStringLiteral("ModernP2P"));
    p2p::core::Logger::install(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                               + QStringLiteral("/logs"));

    // UI language: the "language" setting (e.g. zh_TW) if present, otherwise the system locale.
    const QString forced = QSettings().value(QStringLiteral("language")).toString();
    QTranslator translator;
    if (translator.load(forced.isEmpty() ? QLocale() : QLocale(forced), QStringLiteral("ModernP2P"),
                        QStringLiteral("_"), QStringLiteral(":/i18n")))
        app.installTranslator(&translator);

    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/icon.png")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    if (selftest) {
        if (selftestOut.isEmpty())
            selftestOut = QDir::temp().filePath(QStringLiteral("modernp2p-selftest.txt"));
        QTimer::singleShot(0, &app, [&app, selftestOut] {
            app.exit(p2p::app::runSelfTest(selftestOut, p2p::app::compiledApiBase()));
        });
        return app.exec();
    }

    p2p::app::AppController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("ModernP2P", "Main");

    QTimer::singleShot(0, &controller, &p2p::app::AppController::start);
    return app.exec();
}
