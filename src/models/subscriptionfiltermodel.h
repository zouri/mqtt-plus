#pragma once

#include <QSortFilterProxyModel>
#include <QString>
#include <QVariantMap>

class SubscriptionListModel;

class SubscriptionFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QString filterMode READ filterMode WRITE setFilterMode NOTIFY filterModeChanged)
    Q_PROPERTY(int filterModeIndex READ filterModeIndex WRITE setFilterModeIndex NOTIFY filterModeIndexChanged)
    Q_PROPERTY(bool hasFilter READ hasFilter NOTIFY filterChanged)

public:
    explicit SubscriptionFilterModel(QObject *parent = nullptr);

    int count() const;
    QString filterText() const;
    QString filterMode() const;
    int filterModeIndex() const;
    bool hasFilter() const;

    void setSourceModel(QAbstractItemModel *sourceModel) override;
    void setFilterText(const QString &filterText);
    void setFilterMode(const QString &filterMode);
    void setFilterModeIndex(int index);

    Q_INVOKABLE QVariantMap rowAt(int row) const;

signals:
    void countChanged();
    void filterTextChanged();
    void filterModeChanged();
    void filterModeIndexChanged();
    void filterChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    void connectCountSignals();
    static QString normalizedFilterMode(const QString &filterMode);
    static int filterModeIndexForMode(const QString &filterMode);
    bool modeAcceptsRow(const QModelIndex &sourceIndex) const;
    bool textAcceptsRow(const QModelIndex &sourceIndex) const;

    QString m_filterText;
    QString m_filterMode = QStringLiteral("all");
};
