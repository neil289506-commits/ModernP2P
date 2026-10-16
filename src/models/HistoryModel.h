#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QTimer>

namespace p2p::models {

/// Finished transfers, newest first, persisted as JSON (capped).
class HistoryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles { NameRole = Qt::UserRole + 1, PeerRole, SizeRole, SendingRole, ResultRole, ErrorRole,
                 WhenRole, PathRole };
    static constexpr int kMaxEntries = 300;

    struct Entry
    {
        QString name, peer, result, error, path;
        qint64 size = 0;
        bool sending = false;
        QDateTime when;
    };

    explicit HistoryModel(const QString &filePath, QObject *parent = nullptr);
    ~HistoryModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return static_cast<int>(m_entries.size()); }

    void add(const Entry &entry);
    Q_INVOKABLE void clear();
    void flush();   ///< writes pending changes now

signals:
    void countChanged();

private:
    void load();
    void scheduleSave();

    QString m_path;
    QList<Entry> m_entries;
    QTimer m_saveTimer;
};

} // namespace p2p::models
