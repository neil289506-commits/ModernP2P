#pragma once

#include <QString>

namespace p2p::core {

/// The 8-character device code (A-Z, 0-9).
class CodeGenerator
{
public:
    static constexpr int kCodeLength = 8;

    static QString generate();
    static bool isValid(const QString &code);
    /// Trims and upper-cases user input so codes can be typed in any case.
    static QString normalize(const QString &input);
    /// Per-launch PeerJS id: "mp2p-<CODE>-<12 random a-z0-9>".
    static QString makePeerId(const QString &code);
};

} // namespace p2p::core
