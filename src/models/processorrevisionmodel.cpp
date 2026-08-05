#include "processorrevisionmodel.h"

ProcessorRevisionModel::ProcessorRevisionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ProcessorRevisionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int ProcessorRevisionModel::count() const
{
    return rowCount();
}

QVariant ProcessorRevisionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const QVariantMap row = m_rows.at(index.row()).toMap();
    switch (role) {
    case IdRole:
        return row.value(QStringLiteral("id"));
    case RevisionNumberRole:
        return row.value(QStringLiteral("revisionNumber"));
    case LanguageIdRole:
        return row.value(QStringLiteral("languageId"));
    case LanguageNameRole:
        return row.value(QStringLiteral("languageName"));
    case RuntimeIdRole:
        return row.value(QStringLiteral("runtimeId"));
    case EntryFileRole:
        return row.value(QStringLiteral("entryFile"));
    case EntrySymbolRole:
        return row.value(QStringLiteral("entrySymbol"));
    case CreatedAtRole:
        return row.value(QStringLiteral("createdAt"));
    case CurrentRole:
        return row.value(QStringLiteral("current"));
    case ReadinessStateRole:
        return row.value(QStringLiteral("readinessState"));
    case ReadinessDetailRole:
        return row.value(QStringLiteral("readinessDetail"));
    case SelectableRole:
        return row.value(QStringLiteral("selectable"));
    default:
        return {};
    }
}

QHash<int, QByteArray> ProcessorRevisionModel::roleNames() const
{
    static const QHash<int, QByteArray> roles {
        {IdRole, "id"},
        {RevisionNumberRole, "revisionNumber"},
        {LanguageIdRole, "languageId"},
        {LanguageNameRole, "languageName"},
        {RuntimeIdRole, "runtimeId"},
        {EntryFileRole, "entryFile"},
        {EntrySymbolRole, "entrySymbol"},
        {CreatedAtRole, "createdAt"},
        {CurrentRole, "current"},
        {ReadinessStateRole, "readinessState"},
        {ReadinessDetailRole, "readinessDetail"},
        {SelectableRole, "selectable"},
    };
    return roles;
}

QVariantMap ProcessorRevisionModel::rowAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).toMap() : QVariantMap {};
}

int ProcessorRevisionModel::indexOfId(const QString &id) const
{
    for (qsizetype row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row).toMap().value(QStringLiteral("id")).toString() == id) {
            return static_cast<int>(row);
        }
    }
    return -1;
}

void ProcessorRevisionModel::setRows(const QVariantList &rows)
{
    const bool rowCountChanged = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = rows;
    endResetModel();
    if (rowCountChanged) {
        emit countChanged();
    }
}
