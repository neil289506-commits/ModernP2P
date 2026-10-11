#pragma once

#include <QAbstractListModel>
#include <QJsonObject>

namespace p2p::models {

/// Incoming transfer requests waiting for the user to accept or reject.
class RequestsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles { IdRole = Qt::UserRole + 1, FromRole, FileCountRole, TotalSizeRole, NamesRole };

    explicit RequestsModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_items.size()); }

    /// `request` is the Worker's TransferRequest JSON. Duplicate ids are ignored.
    bool add(const QJsonObject &request);
    void setAll(const QJsonArray &requests);
    /// Removes and returns the request (empty object if unknown).
    QJsonObject take(const QString &id);

signals:
    void countChanged();

private:
    QList<QJsonObject> m_items;
};

} // namespace p2p::models
