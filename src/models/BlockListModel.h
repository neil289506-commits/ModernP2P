#pragma once

#include <QAbstractListModel>
#include <QStringList>

namespace p2p::models {

/// Codes whose requests are rejected automatically. Persisted as JSON.
class BlockListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles { CodeRole = Qt::UserRole + 1 };

    explicit BlockListModel(const QString &filePath, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return static_cast<int>(m_codes.size()); }

    bool contains(const QString &code) const { return m_codes.contains(code); }
    bool add(const QString &code);
    Q_INVOKABLE void remove(const QString &code);

signals:
    void countChanged();

private:
    void save() const;

    QString m_path;
    QStringList m_codes;
};

} // namespace p2p::models
