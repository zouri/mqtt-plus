#include "scriptlibrarymodel.h"

ScriptLibraryModel::ScriptLibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ScriptLibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int ScriptLibraryModel::count() const
{
    return rowCount();
}

QVariant ScriptLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const ScriptLibraryRow &row = m_rows.at(index.row());
    switch (role) {
    case IdRole:
        return row.id;
    case NameRole:
        return row.name;
    case CodeRole:
        return row.code;
    case UpdatedAtRole:
        return row.updatedAt;
    case FilePathRole:
        return row.filePath;
    default:
        return {};
    }
}

QHash<int, QByteArray> ScriptLibraryModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {CodeRole, "code"},
        {UpdatedAtRole, "updatedAt"},
        {FilePathRole, "filePath"},
    };
}

QVariantMap ScriptLibraryModel::rowAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }
    return rowToMap(m_rows.at(row));
}

int ScriptLibraryModel::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

void ScriptLibraryModel::setRows(const QVector<ScriptLibraryRow> &rows)
{
    const bool countWillChange = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = rows;
    endResetModel();
    if (countWillChange) {
        emit countChanged();
    }
}

QVariantMap ScriptLibraryModel::rowToMap(const ScriptLibraryRow &row) const
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), row.id);
    map.insert(QStringLiteral("name"), row.name);
    map.insert(QStringLiteral("code"), row.code);
    map.insert(QStringLiteral("updatedAt"), row.updatedAt);
    map.insert(QStringLiteral("filePath"), row.filePath);
    return map;
}
