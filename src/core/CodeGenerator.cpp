#include "CodeGenerator.h"

#include <QRandomGenerator>

namespace p2p::core {

namespace {
constexpr QLatin1StringView kCodeAlphabet{"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"};
constexpr QLatin1StringView kIdAlphabet{"abcdefghijklmnopqrstuvwxyz0123456789"};

QString randomString(QLatin1StringView alphabet, int length)
{
    QString out;
    out.reserve(length);
    auto *rng = QRandomGenerator::system();
    for (int i = 0; i < length; ++i)
        out.append(alphabet[rng->bounded(static_cast<int>(alphabet.size()))]);
    return out;
}
} // namespace

QString CodeGenerator::generate()
{
    return randomString(kCodeAlphabet, kCodeLength);
}

bool CodeGenerator::isValid(const QString &code)
{
    if (code.size() != kCodeLength)
        return false;
    for (const QChar ch : code) {
        if (!kCodeAlphabet.contains(ch))
            return false;
    }
    return true;
}

QString CodeGenerator::normalize(const QString &input)
{
    return input.trimmed().toUpper();
}

QString CodeGenerator::makePeerId(const QString &code)
{
    return QStringLiteral("mp2p-") + code + QLatin1Char('-') + randomString(kIdAlphabet, 12);
}

} // namespace p2p::core
