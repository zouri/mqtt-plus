#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

class EventStreamModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        KindRole,
        TimestampRole,
        TitleRole,
        PayloadRole,
        PayloadFormatRole,
        PayloadSizeRole,
        TopicRole,
        TestPayloadRole,
        TestFormatRole,
        TestFormatNameRole
    };
    Q_ENUM(Role)

    explicit EventStreamModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;

    void setRows(const QVariantList &rows);
    void appendRow(const QVariantMap &row);
    void prependRows(const QVariantList &rows);
    void clear();
    void trimToLimit(int limit);

signals:
    void countChanged();

private:
    QVariant roleValue(const QVariantMap &row, int role) const;

    QVariantList m_rows;
};
