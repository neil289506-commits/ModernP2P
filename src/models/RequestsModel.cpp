#include "RequestsModel.h"

#include <QJsonArray>

namespace p2p::models {

int RequestsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant RequestsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const QJsonObject &r = m_items.at(index.row());
    const QJsonArray files = r.value(QStringLiteral("files")).toArray();
    switch (role) {
    case IdRole: return r.value(QStringLiteral("id")).toString();
    case FromRole: return r.value(QStringLiteral("from")).toString();
    case FileCountRole: return static_cast<int>(files.size());
    case TotalSizeRole: {
        qint64 total = 0;
        for (const QJsonValue &f : files)
            total += static_cast<qint64>(f.toObject().value(QStringLiteral("size")).toDouble());
        return total;
    }
    case NamesRole: {
        QStringList names;
        for (const QJsonValue &f : files)
            names << f.toObject().value(QStringLiteral("name")).toString();
        return names.join(QStringLiteral(", "));
    }
    default: return {};
    }
}

QHash<int, QByteArray> RequestsModel::roleNames() const
{
    return {{IdRole, "requestId"}, {FromRole, "from"}, {FileCountRole, "fileCount"},
            {TotalSizeRole, "totalSize"}, {NamesRole, "names"}};
}

bool RequestsModel::add(const QJsonObject &request)
{
    const QString id = request.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return false;
    for (const QJsonObject &r : std::as_const(m_items)) {
        if (r.value(QStringLiteral("id")).toString() == id)
            return false;
    }
    beginInsertRows({}, static_cast<int>(m_items.size()), static_cast<int>(m_items.size()));
    m_items.append(request);
    endInsertRows();
    emit countChanged();
    return true;
}

void RequestsModel::setAll(const QJsonArray &requests)
{
    for (const QJsonValue &v : requests)
        add(v.toObject());
}

QJsonObject RequestsModel::take(const QString &id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).value(QStringLiteral("id")).toString() == id) {
            beginRemoveRows({}, i, i);
            const QJsonObject out = m_items.takeAt(i);
            endRemoveRows();
            emit countChanged();
            return out;
        }
    }
    return {};
}

} // namespace p2p::models
