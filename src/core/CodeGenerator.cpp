#include "CodeGenerator.h"

#include <QRandomGenerator>

namespace p2p::core {

namespace {
constexpr QLatin1StringView kAlphabet{"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"};
}

QString CodeGenerator::generate()
{
    QString code;
    code.reserve(kCodeLength);
    auto *rng = QRandomGenerator::system();
    for (int i = 0; i < kCodeLength; ++i) {
        code.append(kAlphabet[rng->bounded(static_cast<int>(kAlphabet.size()))]);
    }
    return code;
}

bool CodeGenerator::isValid(const QString &code)
{
    if (code.size() != kCodeLength)
        return false;
    for (const QChar ch : code) {
        if (!kAlphabet.contains(ch))
            return false;
    }
    return true;
}

QString CodeGenerator::normalize(const QString &input)
{
    return input.trimmed().toUpper();
}

} // namespace p2p::core
