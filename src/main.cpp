#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>

#include "app/AppController.h"

int main(int argc, char *argv[])
{
    // Background pages must not be throttled or the PeerJS host would stall.
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
            "--disable-background-timer-throttling --disable-renderer-backgrounding");
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ModernP2P"));
    QGuiApplication::setOrganizationName(QStringLiteral("ModernP2P"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/icon.png")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    p2p::app::AppController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("ModernP2P", "Main");

    QTimer::singleShot(0, &controller, &p2p::app::AppController::start);
    return app.exec();
}
