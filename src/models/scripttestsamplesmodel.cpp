#include "scripttestsamplesmodel.h"

ScriptTestSamplesModel::ScriptTestSamplesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ScriptTestSamplesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int ScriptTestSamplesModel::count() const
{
    return rowCount();
}

QVariant ScriptTestSamplesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const ScriptTestSampleRow &row = m_rows.at(index.row());
    switch (role) {
    case TopicRole:
        return row.topic;
    case PayloadRole:
        return row.payload;
    case FormatRole:
        return row.format;
    case FormatNameRole:
        return row.formatName;
    case TimestampRole:
        return row.timestamp;
    case PayloadSizeRole:
        return row.payloadSize;
    default:
        return {};
    }
}

QHash<int, QByteArray> ScriptTestSamplesModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {TopicRole, "topic"},
        {PayloadRole, "payload"},
        {FormatRole, "format"},
        {FormatNameRole, "formatName"},
        {TimestampRole, "timestamp"},
        {PayloadSizeRole, "payloadSize"},
    };
    return roles;
}

QVariantMap ScriptTestSamplesModel::rowAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }
    return rowToMap(m_rows.at(row));
}

void ScriptTestSamplesModel::setRows(const QVector<ScriptTestSampleRow> &rows)
{
    const bool countWillChange = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = rows;
    endResetModel();
    if (countWillChange) {
        emit countChanged();
    }
}

QVariantMap ScriptTestSamplesModel::rowToMap(const ScriptTestSampleRow &row) const
{
    QVariantMap map;
    map.insert(QStringLiteral("topic"), row.topic);
    map.insert(QStringLiteral("payload"), row.payload);
    map.insert(QStringLiteral("format"), row.format);
    map.insert(QStringLiteral("formatName"), row.formatName);
    map.insert(QStringLiteral("timestamp"), row.timestamp);
    map.insert(QStringLiteral("payloadSize"), row.payloadSize);
    return map;
}
