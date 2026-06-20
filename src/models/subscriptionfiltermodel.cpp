#include "subscriptionfiltermodel.h"

#include "subscriptionlistmodel.h"

#include <QStringList>

SubscriptionFilterModel::SubscriptionFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    connectCountSignals();
}

int SubscriptionFilterModel::count() const
{
    return rowCount();
}

QString SubscriptionFilterModel::filterText() const
{
    return m_filterText;
}

QString SubscriptionFilterModel::filterMode() const
{
    return m_filterMode;
}

void SubscriptionFilterModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    QSortFilterProxyModel::setSourceModel(sourceModel);
    connectCountSignals();
    emit countChanged();
}

void SubscriptionFilterModel::setFilterText(const QString &filterText)
{
    const QString trimmedText = filterText.trimmed();
    if (m_filterText == trimmedText) {
        return;
    }

    m_filterText = trimmedText;
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    emit filterTextChanged();
}

void SubscriptionFilterModel::setFilterMode(const QString &filterMode)
{
    const QString normalizedMode = filterMode == QStringLiteral("subscribed")
            || filterMode == QStringLiteral("paused")
        ? filterMode
        : QStringLiteral("all");
    if (m_filterMode == normalizedMode) {
        return;
    }

    m_filterMode = normalizedMode;
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    emit filterModeChanged();
}

QVariantMap SubscriptionFilterModel::rowAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }

    auto *subscriptions = qobject_cast<SubscriptionListModel *>(sourceModel());
    if (!subscriptions) {
        return {};
    }

    const QModelIndex sourceIndex = mapToSource(index(row, 0));
    return subscriptions->rowAt(sourceIndex.row());
}

bool SubscriptionFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    return modeAcceptsRow(sourceIndex) && textAcceptsRow(sourceIndex);
}

void SubscriptionFilterModel::connectCountSignals()
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &SubscriptionFilterModel::countChanged, Qt::UniqueConnection);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &SubscriptionFilterModel::countChanged, Qt::UniqueConnection);
    connect(this, &QAbstractItemModel::modelReset, this, &SubscriptionFilterModel::countChanged, Qt::UniqueConnection);
    connect(this, &QAbstractItemModel::layoutChanged, this, &SubscriptionFilterModel::countChanged, Qt::UniqueConnection);
}

bool SubscriptionFilterModel::modeAcceptsRow(const QModelIndex &sourceIndex) const
{
    if (m_filterMode == QStringLiteral("all")) {
        return true;
    }

    const bool paused = sourceIndex.data(SubscriptionListModel::PausedRole).toBool();
    if (m_filterMode == QStringLiteral("paused")) {
        return paused;
    }
    return !paused;
}

bool SubscriptionFilterModel::textAcceptsRow(const QModelIndex &sourceIndex) const
{
    if (m_filterText.isEmpty()) {
        return true;
    }

    const QString needle = m_filterText.toCaseFolded();
    const QStringList haystack = {
        sourceIndex.data(SubscriptionListModel::TopicRole).toString(),
        sourceIndex.data(SubscriptionListModel::AliasRole).toString(),
        sourceIndex.data(SubscriptionListModel::DisplayNameRole).toString(),
        sourceIndex.data(SubscriptionListModel::FormatNameRole).toString(),
    };
    for (const QString &value : haystack) {
        if (value.toCaseFolded().contains(needle)) {
            return true;
        }
    }
    return false;
}
