#pragma once

#include <QDir>
#include <QString>

namespace p2p::core {

/// Makes a name received from a peer safe to create on Windows: strips any
/// directory part, replaces illegal characters, avoids reserved device names
/// and trailing dots/spaces, and limits the length.
QString sanitizeFileName(const QString &input);

/// Returns a path in `dir` that does not collide with an existing file (or a
/// pending ".part" file), appending " (n)" before the extension when needed.
QString uniqueFilePath(const QDir &dir, const QString &fileName);

} // namespace p2p::core
