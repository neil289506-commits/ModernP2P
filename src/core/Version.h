#pragma once

#include <QString>

namespace p2p::core {

/// True if `latest` ("v1.2.3", "1.2") is a higher release than `current`.
/// Pre-release suffixes ("-beta") are ignored; unparsable input returns false.
bool isNewerVersion(const QString &latest, const QString &current);

} // namespace p2p::core
