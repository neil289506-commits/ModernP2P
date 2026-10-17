#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QStringList>

namespace p2p::models {

/// Files dropped into the staging area, waiting to be sent.
class StagingModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(qint64 totalSize READ totalSize NOTIFY countChanged)
public:
    enum Roles { NameRole = Qt::UserRole + 1, SizeRole, PathRole };

    explicit StagingModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_items.size()); }
    qint64 totalSize() const;

    static constexpr int kMaxFiles = 2000;   // keep in sync with MAX_FILES in worker/src/validate.ts

    /// Adds files and, recursively, the files inside folders (announced as
    /// "Folder/sub/file"). Returns how many items were skipped (duplicates,
    /// unreadable, symlinks, or beyond kMaxFiles).
    int addPaths(const QStringList &paths);
    void removeAt(int row);
    void clear();

    QStringList paths() const;
    QStringList names() const;   ///< announced (relative) names, same order as paths()
    QJsonArray toJson() const;   ///< [{name,size}]

signals:
    void countChanged();

private:
    struct Item { QString path; QString name; qint64 size; };
    QList<Item> m_items;
};

} // namespace p2p::models
