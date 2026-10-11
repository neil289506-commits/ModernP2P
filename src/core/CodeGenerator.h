#pragma once

#include <QString>

namespace p2p::core {

/// Generates the 8-character alphanumeric device code (A-Z, 0-9).
class CodeGenerator
{
public:
    static constexpr int kCodeLength = 8;

    /// Returns a new random code using the system CSPRNG.
    static QString generate();

    /// True if `code` is exactly 8 characters of A-Z / 0-9 (case-sensitive).
    static bool isValid(const QString &code);

    /// Trims and upper-cases user input so codes are typed case-insensitively.
    static QString normalize(const QString &input);
};

} // namespace p2p::core
