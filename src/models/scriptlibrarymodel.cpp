#include "scriptlibrarymodel.h"

#include "domain/script.h"
#include "services/apputils.h"
#include "services/storage/scriptstore.h"

#include <utility>

using namespace AppUtils;

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

    const ScriptRow &script = m_rows.at(index.row());
    switch (role) {
    case IdRole:
        return script.id;
    case NameRole:
        return script.name;
    case DescriptionRole:
        return script.description;
    case CodeRole:
        return script.code;
    case UpdatedAtRole:
        return script.updatedAt;
    case FilePathRole:
        return script.filePath;
    default:
        return {};
    }
}

QHash<int, QByteArray> ScriptLibraryModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {IdRole, "id"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {CodeRole, "code"},
        {UpdatedAtRole, "updatedAt"},
        {FilePathRole, "filePath"},
    };
    return roles;
}

QVariantMap ScriptLibraryModel::rowAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }

    const ScriptRow &script = m_rows.at(row);
    QVariantMap map;
    map.insert(QStringLiteral("id"), script.id);
    map.insert(QStringLiteral("name"), script.name);
    map.insert(QStringLiteral("description"), script.description);
    map.insert(QStringLiteral("code"), script.code);
    map.insert(QStringLiteral("updatedAt"), script.updatedAt);
    map.insert(QStringLiteral("filePath"), script.filePath);
    return map;
}

int ScriptLibraryModel::indexOfId(const QString &id) const
{
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void ScriptLibraryModel::setScripts(const QVector<ScriptEntry> &scripts)
{
    QVector<ScriptRow> rows;
    rows.reserve(scripts.size());
    for (const ScriptEntry &script : scripts) {
        rows.append(rowFromScript(script));
    }

    if (rows.size() != m_rows.size()) {
        beginResetModel();
        m_rows = std::move(rows);
        endResetModel();
        emit countChanged();
        return;
    }

    m_rows = std::move(rows);
    if (!m_rows.isEmpty()) {
        emit dataChanged(
            index(0, 0),
            index(static_cast<int>(m_rows.size() - 1), 0),
            {
                IdRole,
                NameRole,
                DescriptionRole,
                CodeRole,
                UpdatedAtRole,
                FilePathRole,
            });
    }
}

ScriptLibraryModel::ScriptRow ScriptLibraryModel::rowFromScript(const ScriptEntry &script)
{
    return {
        script.id,
        script.name,
        script.description,
        script.code,
        displayTimestamp(script.updatedAt),
        ScriptStore::scriptFilePath(script.fileName),
    };
}
