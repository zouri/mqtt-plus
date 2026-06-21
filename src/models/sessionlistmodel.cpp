#include "sessionlistmodel.h"

SessionListModel::SessionListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SessionListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int SessionListModel::count() const
{
    return rowCount();
}

QVariant SessionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const SessionListRow &row = m_rows.at(index.row());
    switch (role) {
    case IdRole:
        return row.id;
    case NameRole:
        return row.name;
    case StateRole:
        return row.state;
    case ConnectedRole:
        return row.connected;
    case HostRole:
        return row.host;
    case PortRole:
        return row.port;
    case TransportRole:
        return row.transport;
    case TransportLabelRole:
        return row.transportLabel;
    case ProtocolVersionRole:
        return row.protocolVersion;
    case ProtocolVersionNameRole:
        return row.protocolVersionName;
    case SummaryRole:
        return row.summary;
    case LastErrorRole:
        return row.lastError;
    default:
        return {};
    }
}

QHash<int, QByteArray> SessionListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {IdRole, "id"},
        {NameRole, "name"},
        {StateRole, "connectionState"},
        {ConnectedRole, "connected"},
        {HostRole, "host"},
        {PortRole, "port"},
        {TransportRole, "transport"},
        {TransportLabelRole, "transportLabel"},
        {ProtocolVersionRole, "protocolVersion"},
        {ProtocolVersionNameRole, "protocolVersionName"},
        {SummaryRole, "summary"},
        {LastErrorRole, "lastError"},
    };
    return roles;
}

QVariantMap SessionListModel::rowAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }
    return rowToMap(m_rows.at(row));
}

void SessionListModel::setRows(const QVector<SessionListRow> &rows)
{
    const bool countWillChange = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = rows;
    endResetModel();
    if (countWillChange) {
        emit countChanged();
    }
}

QVariantMap SessionListModel::rowToMap(const SessionListRow &row) const
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), row.id);
    map.insert(QStringLiteral("name"), row.name);
    map.insert(QStringLiteral("state"), row.state);
    map.insert(QStringLiteral("connected"), row.connected);
    map.insert(QStringLiteral("host"), row.host);
    map.insert(QStringLiteral("port"), row.port);
    map.insert(QStringLiteral("transport"), row.transport);
    map.insert(QStringLiteral("transportLabel"), row.transportLabel);
    map.insert(QStringLiteral("protocolVersion"), row.protocolVersion);
    map.insert(QStringLiteral("protocolVersionName"), row.protocolVersionName);
    map.insert(QStringLiteral("summary"), row.summary);
    map.insert(QStringLiteral("lastError"), row.lastError);
    return map;
}
