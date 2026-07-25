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

int SubscriptionFilterModel::filterModeIndex() const
{
    return filterModeIndexForMode(m_filterMode);
}

bool SubscriptionFilterModel::hasFilter() const
{
    return !m_filterText.isEmpty() || m_filterMode != QStringLiteral("all");
}

void SubscriptionFilterModel::setFilterText(const QString &filterText)
{
    const bool hadFilter = hasFilter();
    const QString trimmedText = filterText.trimmed();
    if (m_filterText == trimmedText) {
        return;
    }

    m_filterText = trimmedText;
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    emit filterTextChanged();
    if (hadFilter != hasFilter()) {
        emit filterChanged();
    }
}

void SubscriptionFilterModel::setFilterMode(const QString &filterMode)
{
    const bool hadFilter = hasFilter();
    const QString normalizedMode = normalizedFilterMode(filterMode);
    if (m_filterMode == normalizedMode) {
        return;
    }

    m_filterMode = normalizedMode;
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    emit filterModeChanged();
    emit filterModeIndexChanged();
    if (hadFilter != hasFilter()) {
        emit filterChanged();
    }
}

void SubscriptionFilterModel::setFilterModeIndex(int index)
{
    static const QStringList modes {
        QStringLiteral("all"),
        QStringLiteral("subscribed"),
        QStringLiteral("paused"),
    };
    setFilterMode(index >= 0 && index < modes.size() ? modes.at(index) : QStringLiteral("all"));
}

QVariantMap SubscriptionFilterModel::rowAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }

    const QModelIndex rowIndex = index(row, 0);
    QVariantMap result;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto role = roles.cbegin(); role != roles.cend(); ++role) {
        QString key = QString::fromUtf8(role.value());
        if (role.key() == SubscriptionListModel::ColorRole) {
            key = QStringLiteral("color");
        } else if (role.key() == SubscriptionListModel::StateRole) {
            key = QStringLiteral("state");
        }
        result.insert(key, rowIndex.data(role.key()));
    }
    return result;
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

QString SubscriptionFilterModel::normalizedFilterMode(const QString &filterMode)
{
    return filterMode == QStringLiteral("subscribed") || filterMode == QStringLiteral("paused")
        ? filterMode
        : QStringLiteral("all");
}

int SubscriptionFilterModel::filterModeIndexForMode(const QString &filterMode)
{
    const QString normalizedMode = normalizedFilterMode(filterMode);
    if (normalizedMode == QStringLiteral("subscribed")) {
        return 1;
    }
    if (normalizedMode == QStringLiteral("paused")) {
        return 2;
    }
    return 0;
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
