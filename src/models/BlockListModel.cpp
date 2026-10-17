#include "BlockListModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace p2p::models {

BlockListModel::BlockListModel(const QString &filePath, QObject *parent)
    : QAbstractListModel(parent)
    , m_path(filePath)
{
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) {
        for (const QJsonValue &v : QJsonDocument::fromJson(f.readAll()).array()) {
            const QString code = v.toString();
            if (!code.isEmpty() && !m_codes.contains(code))
                m_codes << code;
        }
    }
}

int BlockListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_codes.size());
}

QVariant BlockListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_codes.size() || role != CodeRole)
        return {};
    return m_codes.at(index.row());
}

QHash<int, QByteArray> BlockListModel::roleNames() const
{
    return {{CodeRole, "code"}};
}

bool BlockListModel::add(const QString &code)
{
    if (code.isEmpty() || m_codes.contains(code))
        return false;
    beginInsertRows({}, 0, 0);
    m_codes.prepend(code);
    endInsertRows();
    emit countChanged();
    save();
    return true;
}

void BlockListModel::remove(const QString &code)
{
    const int i = static_cast<int>(m_codes.indexOf(code));
    if (i < 0)
        return;
    beginRemoveRows({}, i, i);
    m_codes.removeAt(i);
    endRemoveRows();
    emit countChanged();
    save();
}

void BlockListModel::save() const
{
    QSaveFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(QJsonArray::fromStringList(m_codes)).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

} // namespace p2p::models
