#include "subscriptionlistmodel.h"

SubscriptionListModel::SubscriptionListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SubscriptionListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int SubscriptionListModel::count() const
{
    return rowCount();
}

QVariant SubscriptionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const SubscriptionListRow &row = m_rows.at(index.row());
    switch (role) {
    case TopicRole:
        return row.topic;
    case AliasRole:
        return row.alias;
    case DisplayNameRole:
        return row.displayName;
    case RequestedQosRole:
        return row.requestedQos;
    case GrantedQosRole:
        return row.grantedQos;
    case TopicFpsRole:
        return row.topicFps;
    case FormatRole:
        return row.format;
    case FormatNameRole:
        return row.formatName;
    case ScriptIdRole:
        return row.scriptId;
    case ScriptNameRole:
        return row.scriptName;
    case PausedRole:
        return row.paused;
    case StateRole:
        return row.state;
    case LastErrorRole:
        return row.lastError;
    default:
        return {};
    }
}

QHash<int, QByteArray> SubscriptionListModel::roleNames() const
{
    return {
        {TopicRole, "topic"},
        {AliasRole, "alias"},
        {DisplayNameRole, "displayName"},
        {RequestedQosRole, "requestedQos"},
        {GrantedQosRole, "grantedQos"},
        {TopicFpsRole, "topicFps"},
        {FormatRole, "format"},
        {FormatNameRole, "formatName"},
        {ScriptIdRole, "scriptId"},
        {ScriptNameRole, "scriptName"},
        {PausedRole, "paused"},
        {StateRole, "subscriptionState"},
        {LastErrorRole, "lastError"},
    };
}

QVariantMap SubscriptionListModel::rowAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }
    return rowToMap(m_rows.at(row));
}

void SubscriptionListModel::setRows(const QVector<SubscriptionListRow> &rows)
{
    const bool countWillChange = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = rows;
    endResetModel();
    if (countWillChange) {
        emit countChanged();
    }
}

QVariantMap SubscriptionListModel::rowToMap(const SubscriptionListRow &row) const
{
    QVariantMap map;
    map.insert(QStringLiteral("topic"), row.topic);
    map.insert(QStringLiteral("alias"), row.alias);
    map.insert(QStringLiteral("displayName"), row.displayName);
    map.insert(QStringLiteral("requestedQos"), row.requestedQos);
    map.insert(QStringLiteral("grantedQos"), row.grantedQos);
    map.insert(QStringLiteral("topicFps"), row.topicFps);
    map.insert(QStringLiteral("format"), row.format);
    map.insert(QStringLiteral("formatName"), row.formatName);
    map.insert(QStringLiteral("scriptId"), row.scriptId);
    map.insert(QStringLiteral("scriptName"), row.scriptName);
    map.insert(QStringLiteral("paused"), row.paused);
    map.insert(QStringLiteral("state"), row.state);
    map.insert(QStringLiteral("lastError"), row.lastError);
    return map;
}
