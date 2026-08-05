#pragma once

#include <QAbstractListModel>
#include <QVariantList>

class ProcessorRevisionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role : int {
        IdRole = Qt::UserRole + 1,
        RevisionNumberRole,
        LanguageIdRole,
        LanguageNameRole,
        RuntimeIdRole,
        EntryFileRole,
        EntrySymbolRole,
        CreatedAtRole,
        CurrentRole,
        ReadinessStateRole,
        ReadinessDetailRole,
        SelectableRole,
    };
    Q_ENUM(Role)

    explicit ProcessorRevisionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;
    Q_INVOKABLE int indexOfId(const QString &id) const;

    void setRows(const QVariantList &rows);

signals:
    void countChanged();

private:
    QVariantList m_rows;
};
