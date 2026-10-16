#include "FileNames.h"

#include <QFileInfo>
#include <QStringList>

namespace p2p::core {

QString sanitizeFileName(const QString &input)
{
    QString name = input;
    const qsizetype slash = qMax(name.lastIndexOf(QLatin1Char('/')), name.lastIndexOf(QLatin1Char('\\')));
    if (slash >= 0)
        name = name.mid(slash + 1);

    static const QString illegal = QStringLiteral("<>:\"/\\|?*");
    QString out;
    out.reserve(name.size());
    for (const QChar c : name)
        out.append((c.unicode() < 32 || illegal.contains(c)) ? QLatin1Char('_') : c);

    while (out.endsWith(QLatin1Char('.')) || out.endsWith(QLatin1Char(' ')))
        out.chop(1);
    while (out.startsWith(QLatin1Char(' ')))
        out.remove(0, 1);

    constexpr int kMaxLength = 200;
    if (out.size() > kMaxLength) {
        const QString suffix = QFileInfo(out).suffix();
        if (!suffix.isEmpty() && suffix.size() < 20)
            out = out.left(kMaxLength - suffix.size() - 1) + QLatin1Char('.') + suffix;
        else
            out = out.left(kMaxLength);
    }

    static const QStringList reserved = {
        "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
        "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
    if (reserved.contains(out.section(QLatin1Char('.'), 0, 0).toUpper()))
        out.prepend(QLatin1Char('_'));

    return out.isEmpty() ? QStringLiteral("file") : out;
}

QString uniqueFilePath(const QDir &dir, const QString &fileName)
{
    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();

    QString candidate = dir.filePath(fileName);
    int n = 1;
    while (QFileInfo::exists(candidate) || QFileInfo::exists(candidate + QStringLiteral(".part"))) {
        const QString numbered = suffix.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(base).arg(n)
            : QStringLiteral("%1 (%2).%3").arg(base).arg(n).arg(suffix);
        candidate = dir.filePath(numbered);
        ++n;
    }
    return candidate;
}

QStringList sanitizeRelativePath(const QString &input)
{
    constexpr int kMaxDepth = 16;
    QStringList out;
    const QStringList raw = QString(input).replace(QLatin1Char('\\'), QLatin1Char('/')).split(QLatin1Char('/'));
    for (const QString &part : raw) {
        const QString trimmed = part.trimmed();
        if (trimmed.isEmpty() || trimmed == QLatin1String(".") || trimmed == QLatin1String(".."))
            continue;
        out << sanitizeFileName(trimmed);
    }
    return out.size() > kMaxDepth ? QStringList{} : out;
}

QString uniqueDirName(const QDir &dir, const QString &name)
{
    QString candidate = name;
    for (int n = 1; QFileInfo::exists(dir.filePath(candidate)); ++n)
        candidate = QStringLiteral("%1 (%2)").arg(name).arg(n);
    return candidate;
}

} // namespace p2p::core
