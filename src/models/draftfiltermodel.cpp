#include "models/draftfiltermodel.h"

#include "models/draftlibrarymodel.h"

DraftFilterModel::DraftFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
    connect(this, &QAbstractItemModel::rowsInserted, this, &DraftFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &DraftFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &DraftFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &DraftFilterModel::countChanged);
}

QString DraftFilterModel::filterText() const { return m_filterText; }
QString DraftFilterModel::sortMode() const { return m_sortMode; }
int DraftFilterModel::count() const { return rowCount(); }

void DraftFilterModel::setFilterText(const QString &filterText)
{
    if (m_filterText == filterText) {
        return;
    }
    m_filterText = filterText;
    const QString normalized = filterText.trimmed();
    if (m_normalizedFilterText != normalized) {
        m_normalizedFilterText = normalized;
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    }
    emit filterTextChanged();
}

void DraftFilterModel::setSortMode(const QString &sortMode)
{
    const QString normalized = sortMode == QStringLiteral("recent")
        ? QStringLiteral("recent")
        : QStringLiteral("name");
    if (m_sortMode == normalized) {
        return;
    }
    m_sortMode = normalized;
    invalidate();
    sort(0, Qt::AscendingOrder);
    emit sortModeChanged();
}

QVariantMap DraftFilterModel::rowAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }
    const QModelIndex rowIndex = index(row, 0);
    QVariantMap result;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        result.insert(QString::fromUtf8(it.value()), rowIndex.data(it.key()));
    }
    return result;
}

bool DraftFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!sourceModel()) {
        return false;
    }
    if (m_normalizedFilterText.isEmpty()) {
        return true;
    }
    const QModelIndex row = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString searchable = QStringLiteral("%1 %2 %3 %4").arg(
        row.data(DraftLibraryModel::NameRole).toString(),
        row.data(DraftLibraryModel::DescriptionRole).toString(),
        row.data(DraftLibraryModel::DefaultTopicRole).toString(),
        row.data(DraftLibraryModel::PayloadRole).toString());
    return searchable.contains(m_normalizedFilterText, Qt::CaseInsensitive);
}

bool DraftFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (m_sortMode == QStringLiteral("recent")) {
        const QString leftUsed = left.data(DraftLibraryModel::LastUsedAtRole).toString();
        const QString rightUsed = right.data(DraftLibraryModel::LastUsedAtRole).toString();
        if (leftUsed != rightUsed) {
            if (leftUsed.isEmpty()) {
                return false;
            }
            if (rightUsed.isEmpty()) {
                return true;
            }
            return leftUsed > rightUsed;
        }
    }
    return QString::localeAwareCompare(
               left.data(DraftLibraryModel::NameRole).toString(),
               right.data(DraftLibraryModel::NameRole).toString()) < 0;
}
