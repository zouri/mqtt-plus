#include "scriptfiltermodel.h"

#include "models/scriptlibrarymodel.h"

ScriptFilterModel::ScriptFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    connect(this, &QAbstractItemModel::rowsInserted, this, &ScriptFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ScriptFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ScriptFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &ScriptFilterModel::countChanged);
}

QString ScriptFilterModel::filterText() const
{
    return m_filterText;
}

int ScriptFilterModel::count() const
{
    return rowCount();
}

void ScriptFilterModel::setFilterText(const QString &filterText)
{
    if (m_filterText == filterText) {
        return;
    }

    const QString normalized = filterText.trimmed();
    const bool filterChanged = m_normalizedFilterText != normalized;
    m_filterText = filterText;
    m_normalizedFilterText = normalized;
    if (filterChanged) {
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    }
    emit filterTextChanged();
}

QVariantMap ScriptFilterModel::rowAt(int row) const
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

bool ScriptFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!sourceModel() || m_normalizedFilterText.isEmpty()) {
        return sourceModel() != nullptr;
    }

    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString searchableText = QStringLiteral("%1 %2 %3").arg(
        sourceIndex.data(ScriptLibraryModel::NameRole).toString(),
        sourceIndex.data(ScriptLibraryModel::DescriptionRole).toString(),
        sourceIndex.data(ScriptLibraryModel::CodeRole).toString());
    return searchableText.contains(m_normalizedFilterText, Qt::CaseInsensitive);
}
