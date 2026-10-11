#pragma once

#include <QAbstractListModel>

namespace p2p::models {

/// One row per file being sent or received.
class TransferModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged)
public:
    enum Roles { IdRole = Qt::UserRole + 1, NameRole, SizeRole, DoneRole, StateRole, SendingRole,
                 PeerRole, SpeedRole, ErrorRole, ActiveRole };

    enum class State { Waiting, Transferring, Verifying, Done, Failed, Cancelled };

    explicit TransferModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int activeCount() const;

    QString add(const QString &name, qint64 size, bool sending, const QString &peerCode);
    void update(const QString &id, qint64 done, State state, double bytesPerSecond = 0,
                const QString &error = {});
    bool isActive(const QString &id) const;

    static QString stateText(State s);

signals:
    void activeCountChanged();

private:
    struct Item {
        QString id, name, peer, error;
        qint64 size = 0, done = 0;
        bool sending = false;
        State state = State::Waiting;
        double speed = 0;
    };
    int indexOf(const QString &id) const;
    static bool active(State s) { return s == State::Waiting || s == State::Transferring || s == State::Verifying; }

    QList<Item> m_items;
    int m_counter = 0;
};

} // namespace p2p::models
