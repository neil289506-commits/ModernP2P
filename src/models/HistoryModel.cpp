#include "HistoryModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace p2p::models {

HistoryModel::HistoryModel(const QString &filePath, QObject *parent)
    : QAbstractListModel(parent)
    , m_path(filePath)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(400);
    connect(&m_saveTimer, &QTimer::timeout, this, &HistoryModel::flush);
    load();
}

HistoryModel::~HistoryModel()
{
    if (m_saveTimer.isActive())
        flush();
}

int HistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant HistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry &e = m_entries.at(index.row());
    switch (role) {
    case NameRole: return e.name;
    case PeerRole: return e.peer;
    case SizeRole: return e.size;
    case SendingRole: return e.sending;
    case ResultRole: return e.result;
    case ErrorRole: return e.error;
    case WhenRole: return e.when.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    case PathRole: return e.path;
    default: return {};
    }
}

QHash<int, QByteArray> HistoryModel::roleNames() const
{
    return {{NameRole, "name"}, {PeerRole, "peer"}, {SizeRole, "size"}, {SendingRole, "sending"},
            {ResultRole, "result"}, {ErrorRole, "error"}, {WhenRole, "when"}, {PathRole, "path"}};
}

void HistoryModel::add(const Entry &entry)
{
    beginInsertRows({}, 0, 0);
    m_entries.prepend(entry);
    endInsertRows();
    if (m_entries.size() > kMaxEntries) {
        beginRemoveRows({}, kMaxEntries, static_cast<int>(m_entries.size()) - 1);
        m_entries.erase(m_entries.begin() + kMaxEntries, m_entries.end());
        endRemoveRows();
    }
    emit countChanged();
    scheduleSave();
}

void HistoryModel::clear()
{
    if (m_entries.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
    scheduleSave();
}

void HistoryModel::scheduleSave()
{
    m_saveTimer.start();
}

void HistoryModel::flush()
{
    m_saveTimer.stop();
    QJsonArray arr;
    for (const Entry &e : std::as_const(m_entries)) {
        arr.append(QJsonObject{{QStringLiteral("name"), e.name}, {QStringLiteral("peer"), e.peer},
                               {QStringLiteral("result"), e.result}, {QStringLiteral("error"), e.error},
                               {QStringLiteral("path"), e.path}, {QStringLiteral("size"), static_cast<double>(e.size)},
                               {QStringLiteral("sending"), e.sending},
                               {QStringLiteral("when"), e.when.toString(Qt::ISODate)}});
    }
    QSaveFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

void HistoryModel::load()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        Entry e;
        e.name = o.value(QStringLiteral("name")).toString();
        e.peer = o.value(QStringLiteral("peer")).toString();
        e.result = o.value(QStringLiteral("result")).toString();
        e.error = o.value(QStringLiteral("error")).toString();
        e.path = o.value(QStringLiteral("path")).toString();
        e.size = static_cast<qint64>(o.value(QStringLiteral("size")).toDouble());
        e.sending = o.value(QStringLiteral("sending")).toBool();
        e.when = QDateTime::fromString(o.value(QStringLiteral("when")).toString(), Qt::ISODate);
        if (!e.name.isEmpty() && m_entries.size() < kMaxEntries)
            m_entries.append(e);
    }
}

} // namespace p2p::models
