#pragma once

#include <QString>

namespace p2p::core {

/// The device's registered code and the secret token proving ownership of it.
/// Persisted with QSettings (HKCU registry on Windows).
struct Identity
{
    QString code;
    QString token;

    bool isValid() const { return !code.isEmpty() && !token.isEmpty(); }

    static Identity load();
    void save() const;
    static void clear();
};

} // namespace p2p::core
