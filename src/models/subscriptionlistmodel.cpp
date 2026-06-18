#include "subscriptionlistmodel.h"

namespace {

QList<int> changedRoles(const SubscriptionListRow &oldRow, const SubscriptionListRow &newRow)
{
    QList<int> roles;
    if (oldRow.topic != newRow.topic) {
        roles.append(SubscriptionListModel::TopicRole);
    }
    if (oldRow.alias != newRow.alias) {
        roles.append(SubscriptionListModel::AliasRole);
    }
    if (oldRow.displayName != newRow.displayName) {
        roles.append(SubscriptionListModel::DisplayNameRole);
    }
    if (oldRow.requestedQos != newRow.requestedQos) {
        roles.append(SubscriptionListModel::RequestedQosRole);
    }
    if (oldRow.grantedQos != newRow.grantedQos) {
        roles.append(SubscriptionListModel::GrantedQosRole);
    }
    if (oldRow.topicFps != newRow.topicFps) {
        roles.append(SubscriptionListModel::TopicFpsRole);
    }
    if (oldRow.format != newRow.format) {
        roles.append(SubscriptionListModel::FormatRole);
    }
    if (oldRow.formatName != newRow.formatName) {
        roles.append(SubscriptionListModel::FormatNameRole);
    }
    if (oldRow.scriptId != newRow.scriptId) {
        roles.append(SubscriptionListModel::ScriptIdRole);
    }
    if (oldRow.scriptName != newRow.scriptName) {
        roles.append(SubscriptionListModel::ScriptNameRole);
    }
    if (oldRow.paused != newRow.paused) {
        roles.append(SubscriptionListModel::PausedRole);
    }
    if (oldRow.state != newRow.state) {
        roles.append(SubscriptionListModel::StateRole);
    }
    if (oldRow.lastError != newRow.lastError) {
        roles.append(SubscriptionListModel::LastErrorRole);
    }
    return roles;
}

bool hasSameIdentityOrder(const QVector<SubscriptionListRow> &oldRows, const QVector<SubscriptionListRow> &newRows)
{
    if (oldRows.size() != newRows.size()) {
        return false;
    }

    for (qsizetype i = 0; i < oldRows.size(); ++i) {
        if (oldRows.at(i).topic != newRows.at(i).topic) {
            return false;
        }
    }

    return true;
}

} // namespace

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
    if (hasSameIdentityOrder(m_rows, rows)) {
        for (qsizetype i = 0; i < rows.size(); ++i) {
            const QList<int> roles = changedRoles(m_rows.at(i), rows.at(i));
            if (roles.isEmpty()) {
                continue;
            }

            m_rows[i] = rows.at(i);
            const QModelIndex rowIndex = index(static_cast<int>(i), 0);
            emit dataChanged(rowIndex, rowIndex, roles);
        }
        return;
    }

    const bool countWillChange = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = rows;
    endResetModel();
    if (countWillChange) {
        emit countChanged();
    }
}

void SubscriptionListModel::setTopicFpsRows(const QVector<SubscriptionFpsRow> &rows)
{
    if (rows.size() != m_rows.size()) {
        return;
    }

    for (qsizetype i = 0; i < rows.size(); ++i) {
        if (m_rows.at(i).topic != rows.at(i).topic || m_rows.at(i).topicFps == rows.at(i).topicFps) {
            continue;
        }

        m_rows[i].topicFps = rows.at(i).topicFps;
        const QModelIndex rowIndex = index(static_cast<int>(i), 0);
        emit dataChanged(rowIndex, rowIndex, {TopicFpsRole});
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
