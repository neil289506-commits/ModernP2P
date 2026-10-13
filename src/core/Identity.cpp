#include "Identity.h"

#include <QSettings>

namespace p2p::core {

Identity Identity::load()
{
    QSettings s;
    return {s.value(QStringLiteral("identity/code")).toString(),
            s.value(QStringLiteral("identity/token")).toString()};
}

void Identity::save() const
{
    QSettings s;
    s.setValue(QStringLiteral("identity/code"), code);
    s.setValue(QStringLiteral("identity/token"), token);
}

void Identity::clear()
{
    QSettings s;
    s.remove(QStringLiteral("identity"));
}

} // namespace p2p::core
