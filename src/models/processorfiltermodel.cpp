#include "processorfiltermodel.h"

#include "models/processorlibrarymodel.h"

ProcessorFilterModel::ProcessorFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    connect(this, &QAbstractItemModel::rowsInserted, this, &ProcessorFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ProcessorFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ProcessorFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &ProcessorFilterModel::countChanged);
}

QString ProcessorFilterModel::filterText() const
{
    return m_filterText;
}

int ProcessorFilterModel::count() const
{
    return rowCount();
}

void ProcessorFilterModel::setFilterText(const QString &filterText)
{
    if (m_filterText == filterText) {
        return;
    }
    const QString normalized = filterText.trimmed();
    const bool normalizedChanged = normalized != m_normalizedFilterText;
    m_filterText = filterText;
    m_normalizedFilterText = normalized;
    if (normalizedChanged) {
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    }
    emit filterTextChanged();
}

QVariantMap ProcessorFilterModel::rowAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }
    const QModelIndex rowIndex = index(row, 0);
    QVariantMap result;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto role = roles.cbegin(); role != roles.cend(); ++role) {
        result.insert(QString::fromUtf8(role.value()), rowIndex.data(role.key()));
    }
    return result;
}

bool ProcessorFilterModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex &sourceParent) const
{
    if (!sourceModel() || m_normalizedFilterText.isEmpty()) {
        return sourceModel() != nullptr;
    }
    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString searchableText = QStringLiteral("%1\n%2\n%3\n%4").arg(
        sourceIndex.data(ProcessorLibraryModel::NameRole).toString(),
        sourceIndex.data(ProcessorLibraryModel::DescriptionRole).toString(),
        sourceIndex.data(ProcessorLibraryModel::LanguageNameRole).toString(),
        sourceIndex.data(ProcessorLibraryModel::SourceTextRole).toString());
    return searchableText.contains(m_normalizedFilterText, Qt::CaseInsensitive);
}
