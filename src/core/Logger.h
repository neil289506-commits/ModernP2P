#pragma once

#include <QString>

namespace p2p::core {

/// Writes Qt log messages to <dir>/app.log, rotating at 1 MiB and keeping
/// app.1.log ... app.3.log.
class Logger
{
public:
    static constexpr qint64 kMaxBytes = 1024 * 1024;
    static constexpr int kKeepFiles = 3;

    static void install(const QString &dir);
    static void uninstall();
    static QString directory();
};

} // namespace p2p::core
