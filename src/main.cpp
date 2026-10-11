#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "core/CodeGenerator.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("ModernP2P");
    QGuiApplication::setOrganizationName("ModernP2P");
    QQuickStyle::setStyle("Basic");

    QQmlApplicationEngine engine;
    // Placeholder: the code is not yet verified against Cloudflare (next PR).
    engine.rootContext()->setContextProperty("localCode", p2p::core::CodeGenerator::generate());

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("ModernP2P", "Main");

    return app.exec();
}
