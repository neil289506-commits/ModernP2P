#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>

namespace p2p::core {

namespace {
QMutex g_mutex;
QString g_dir;
QFile g_file;
QtMessageHandler g_previous = nullptr;
int g_sinceCheck = 0;

QString pathFor(int index)
{
    return QDir(g_dir).filePath(index == 0 ? QStringLiteral("app.log") : QStringLiteral("app.%1.log").arg(index));
}

void rotateLocked()
{
    g_file.close();
    QFile::remove(pathFor(Logger::kKeepFiles));
    for (int i = Logger::kKeepFiles - 1; i >= 0; --i)
        QFile::rename(pathFor(i), pathFor(i + 1));
    g_file.setFileName(pathFor(0));
    g_file.open(QIODevice::WriteOnly | QIODevice::Append);
}

void handler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    {
        QMutexLocker lock(&g_mutex);
        if (g_file.isOpen()) {
            if (++g_sinceCheck >= 50) {
                g_sinceCheck = 0;
                if (g_file.size() > Logger::kMaxBytes)
                    rotateLocked();
            }
            const char *level = type == QtDebugMsg ? "DEBUG" : type == QtInfoMsg ? "INFO"
                              : type == QtWarningMsg ? "WARN" : "ERROR";
            g_file.write(QStringLiteral("%1 [%2] %3\n")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs), QLatin1String(level), msg)
                             .toUtf8());
            g_file.flush();
        }
    }
    if (g_previous)
        g_previous(type, ctx, msg);
}
} // namespace

void Logger::install(const QString &dir)
{
    QMutexLocker lock(&g_mutex);
    if (g_previous || g_file.isOpen())
        return;
    QDir().mkpath(dir);
    g_dir = dir;
    g_file.setFileName(pathFor(0));
    if (!g_file.open(QIODevice::WriteOnly | QIODevice::Append))
        return;
    if (g_file.size() > kMaxBytes)
        rotateLocked();
    g_previous = qInstallMessageHandler(handler);
    if (!g_previous)
        g_previous = [](QtMsgType, const QMessageLogContext &, const QString &) {};   // marks "installed"
}

void Logger::uninstall()
{
    QMutexLocker lock(&g_mutex);
    if (!g_file.isOpen())
        return;
    qInstallMessageHandler(nullptr);
    g_previous = nullptr;
    g_file.close();
}

QString Logger::directory()
{
    return g_dir;
}

} // namespace p2p::core
