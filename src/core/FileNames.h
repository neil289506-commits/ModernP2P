#pragma once

#include <QDir>
#include <QString>
#include <QStringList>

namespace p2p::core {

/// Makes a name received from a peer safe to create on Windows: strips any
/// directory part, replaces illegal characters, avoids reserved device names
/// and trailing dots/spaces, and limits the length.
QString sanitizeFileName(const QString &input);

/// Returns a path in `dir` that does not collide with an existing file (or a
/// pending ".part" file), appending " (n)" before the extension when needed.
QString uniqueFilePath(const QDir &dir, const QString &fileName);

/// Turns a relative path received from a peer ("Album/sub/a.jpg") into safe
/// components: "." and ".." and empty parts are dropped, every part is
/// sanitised with sanitizeFileName, depth is limited. Never contains separators.
QStringList sanitizeRelativePath(const QString &input);

/// Like uniqueFilePath but for directories (" (n)" appended to the name).
QString uniqueDirName(const QDir &dir, const QString &name);

} // namespace p2p::core
