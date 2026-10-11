#include "StagingModel.h"

#include <QFileInfo>
#include <QJsonObject>

namespace p2p::models {

int StagingModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant StagingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Item &it = m_items.at(index.row());
    switch (role) {
    case NameRole: return it.name;
    case SizeRole: return it.size;
    case PathRole: return it.path;
    default: return {};
    }
}

QHash<int, QByteArray> StagingModel::roleNames() const
{
    return {{NameRole, "name"}, {SizeRole, "size"}, {PathRole, "path"}};
}

qint64 StagingModel::totalSize() const
{
    qint64 total = 0;
    for (const Item &it : m_items)
        total += it.size;
    return total;
}

int StagingModel::addPaths(const QStringList &paths)
{
    int skipped = 0;
    for (const QString &p : paths) {
        const QFileInfo fi(p);
        bool duplicate = false;
        for (const Item &it : std::as_const(m_items))
            duplicate = duplicate || it.path == fi.absoluteFilePath();
        if (!fi.isFile() || !fi.isReadable() || duplicate) {
            ++skipped;
            continue;
        }
        beginInsertRows({}, static_cast<int>(m_items.size()), static_cast<int>(m_items.size()));
        m_items.append({fi.absoluteFilePath(), fi.fileName(), fi.size()});
        endInsertRows();
    }
    emit countChanged();
    return skipped;
}

void StagingModel::removeAt(int row)
{
    if (row < 0 || row >= m_items.size())
        return;
    beginRemoveRows({}, row, row);
    m_items.removeAt(row);
    endRemoveRows();
    emit countChanged();
}

void StagingModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
}

QStringList StagingModel::paths() const
{
    QStringList out;
    for (const Item &it : m_items)
        out << it.path;
    return out;
}

QJsonArray StagingModel::toJson() const
{
    QJsonArray arr;
    for (const Item &it : m_items)
        arr.append(QJsonObject{{QStringLiteral("name"), it.name}, {QStringLiteral("size"), static_cast<double>(it.size)}});
    return arr;
}

} // namespace p2p::models
