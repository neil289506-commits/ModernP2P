#include "Version.h"

#include <QList>

namespace p2p::core {

namespace {
bool parse(QString v, QList<int> &out)
{
    v = v.trimmed();
    if (v.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        v.remove(0, 1);
    v = v.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('+'), 0, 0);
    if (v.isEmpty())
        return false;
    for (const QString &part : v.split(QLatin1Char('.'))) {
        bool ok = false;
        const int n = part.toInt(&ok);
        if (!ok || n < 0)
            return false;
        out << n;
    }
    return true;
}
} // namespace

bool isNewerVersion(const QString &latest, const QString &current)
{
    QList<int> a, b;
    if (!parse(latest, a) || !parse(current, b))
        return false;
    const qsizetype n = qMax(a.size(), b.size());
    for (qsizetype i = 0; i < n; ++i) {
        const int x = i < a.size() ? a.at(i) : 0;
        const int y = i < b.size() ? b.at(i) : 0;
        if (x != y)
            return x > y;
    }
    return false;
}

} // namespace p2p::core
