#include "scriptlibrarymodel.h"

#include "domain/script.h"
#include "services/apputils.h"
#include "services/storage/scriptstore.h"

using namespace AppUtils;

ScriptLibraryModel::ScriptLibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ScriptLibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !m_scripts ? 0 : m_scripts->size();
}

int ScriptLibraryModel::count() const
{
    return rowCount();
}

QVariant ScriptLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !m_scripts || index.row() < 0 || index.row() >= m_scripts->size()) {
        return {};
    }

    const auto &script = m_scripts->at(index.row());
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
        return displayTimestamp(script.updatedAt);
    case FilePathRole:
        return ScriptStore::scriptFilePath(script.fileName);
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
    if (!m_scripts || row < 0 || row >= m_scripts->size()) {
        return {};
    }

    const auto &script = m_scripts->at(row);
    QVariantMap map;
    map.insert(QStringLiteral("id"), script.id);
    map.insert(QStringLiteral("name"), script.name);
    map.insert(QStringLiteral("description"), script.description);
    map.insert(QStringLiteral("code"), script.code);
    map.insert(QStringLiteral("updatedAt"), displayTimestamp(script.updatedAt));
    map.insert(QStringLiteral("filePath"), ScriptStore::scriptFilePath(script.fileName));
    return map;
}

int ScriptLibraryModel::indexOfId(const QString &id) const
{
    if (!m_scripts) {
        return -1;
    }
    for (qsizetype i = 0; i < m_scripts->size(); ++i) {
        if (m_scripts->at(i).id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void ScriptLibraryModel::setSource(const QVector<ScriptEntry> *scripts)
{
    m_scripts = scripts;
    beginResetModel();
    endResetModel();
    m_knownCount = count();
    emit countChanged();
}

void ScriptLibraryModel::notifyRefresh()
{
    const int refreshedCount = count();
    if (refreshedCount != m_knownCount) {
        beginResetModel();
        endResetModel();
        m_knownCount = refreshedCount;
        emit countChanged();
        return;
    }

    if (refreshedCount > 0) {
        emit dataChanged(
            index(0, 0),
            index(refreshedCount - 1, 0),
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
