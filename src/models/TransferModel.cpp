#include "TransferModel.h"

namespace p2p::models {

int TransferModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant TransferModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Item &it = m_items.at(index.row());
    switch (role) {
    case IdRole: return it.id;
    case NameRole: return it.name;
    case SizeRole: return it.size;
    case DoneRole: return it.done;
    case StateRole: return stateText(it.state);
    case SendingRole: return it.sending;
    case PeerRole: return it.peer;
    case SpeedRole: return it.speed;
    case ErrorRole: return it.error;
    case ActiveRole: return active(it.state);
    default: return {};
    }
}

QHash<int, QByteArray> TransferModel::roleNames() const
{
    return {{IdRole, "transferId"}, {NameRole, "name"}, {SizeRole, "size"}, {DoneRole, "done"},
            {StateRole, "state"}, {SendingRole, "sending"}, {PeerRole, "peer"}, {SpeedRole, "speed"},
            {ErrorRole, "error"}, {ActiveRole, "active"}};
}

int TransferModel::activeCount() const
{
    int n = 0;
    for (const Item &it : m_items)
        n += active(it.state) ? 1 : 0;
    return n;
}

int TransferModel::indexOf(const QString &id) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id)
            return i;
    }
    return -1;
}

QString TransferModel::add(const QString &name, qint64 size, bool sending, const QString &peerCode)
{
    Item it;
    it.id = QStringLiteral("t%1").arg(++m_counter);
    it.name = name;
    it.size = size;
    it.sending = sending;
    it.peer = peerCode;
    beginInsertRows({}, 0, 0);
    m_items.prepend(it);
    endInsertRows();
    emit activeCountChanged();
    return it.id;
}

void TransferModel::update(const QString &id, qint64 done, State state, double bytesPerSecond,
                           const QString &error)
{
    const int i = indexOf(id);
    if (i < 0)
        return;
    Item &it = m_items[i];
    const bool wasActive = active(it.state);
    it.done = done;
    it.state = state;
    it.speed = bytesPerSecond;
    it.error = error;
    const QModelIndex idx = index(i);
    emit dataChanged(idx, idx);
    if (wasActive != active(state))
        emit activeCountChanged();
}

bool TransferModel::isActive(const QString &id) const
{
    const int i = indexOf(id);
    return i >= 0 && active(m_items.at(i).state);
}

QString TransferModel::stateText(State s)
{
    switch (s) {
    case State::Waiting: return QStringLiteral("Waiting");
    case State::Transferring: return QStringLiteral("Transferring");
    case State::Verifying: return QStringLiteral("Verifying");
    case State::Done: return QStringLiteral("Done");
    case State::Failed: return QStringLiteral("Failed");
    case State::Cancelled: return QStringLiteral("Cancelled");
    }
    return {};
}

} // namespace p2p::models
