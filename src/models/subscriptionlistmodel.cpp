#include "subscriptionlistmodel.h"

namespace {

bool sameSubscriptionIdentity(const QVector<SubscriptionListRow> &left, const QVector<SubscriptionListRow> &right)
{
    if (left.size() != right.size()) {
        return false;
    }

    for (qsizetype i = 0; i < left.size(); ++i) {
        if (left.at(i).topic != right.at(i).topic) {
            return false;
        }
    }

    return true;
}

bool rowsEqual(const SubscriptionListRow &left, const SubscriptionListRow &right)
{
    return left.topic == right.topic
        && left.alias == right.alias
        && left.displayName == right.displayName
        && left.requestedQos == right.requestedQos
        && left.grantedQos == right.grantedQos
        && left.topicFps == right.topicFps
        && left.format == right.format
        && left.formatName == right.formatName
        && left.scriptId == right.scriptId
        && left.scriptName == right.scriptName
        && left.paused == right.paused
        && left.state == right.state
        && left.lastError == right.lastError;
}

const QList<int> &allSubscriptionRoles()
{
    static const QList<int> roles {
        SubscriptionListModel::TopicRole,
        SubscriptionListModel::AliasRole,
        SubscriptionListModel::DisplayNameRole,
        SubscriptionListModel::RequestedQosRole,
        SubscriptionListModel::GrantedQosRole,
        SubscriptionListModel::TopicFpsRole,
        SubscriptionListModel::FormatRole,
        SubscriptionListModel::FormatNameRole,
        SubscriptionListModel::ScriptIdRole,
        SubscriptionListModel::ScriptNameRole,
        SubscriptionListModel::PausedRole,
        SubscriptionListModel::StateRole,
        SubscriptionListModel::LastErrorRole,
    };
    return roles;
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
    const bool countWillChange = rows.size() != m_rows.size();

    if (sameSubscriptionIdentity(m_rows, rows)) {
        for (qsizetype row = 0; row < rows.size(); ++row) {
            if (rowsEqual(m_rows.at(row), rows.at(row))) {
                continue;
            }

            m_rows[row] = rows.at(row);
            const QModelIndex changedIndex = index(static_cast<int>(row), 0);
            emit dataChanged(changedIndex, changedIndex, allSubscriptionRoles());
        }
        return;
    }

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
