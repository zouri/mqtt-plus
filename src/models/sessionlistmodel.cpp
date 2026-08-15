#include "sessionlistmodel.h"

#include "domain/session.h"
#include "domain/sessionconfig.h"
#include "services/apputils.h"

#include <algorithm>
#include <utility>

using namespace AppUtils;

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

    const SessionRow &row = m_rows.at(index.row());

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
    case ClientIdRole:
        return row.clientId;
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
    case UnreadMessageCountRole:
        return row.unreadMessageCount;
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
        {ClientIdRole, "clientId"},
        {TransportRole, "transport"},
        {TransportLabelRole, "transportLabel"},
        {ProtocolVersionRole, "protocolVersion"},
        {ProtocolVersionNameRole, "protocolVersionName"},
        {SummaryRole, "summary"},
        {LastErrorRole, "lastError"},
        {UnreadMessageCountRole, "unreadMessageCount"},
    };
    return roles;
}

QVariantMap SessionListModel::rowAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }

    const SessionRow &session = m_rows.at(row);
    QVariantMap map;
    map.insert(QStringLiteral("id"), session.id);
    map.insert(QStringLiteral("name"), session.name);
    map.insert(QStringLiteral("state"), session.state);
    map.insert(QStringLiteral("connected"), session.connected);
    map.insert(QStringLiteral("host"), session.host);
    map.insert(QStringLiteral("port"), session.port);
    map.insert(QStringLiteral("clientId"), session.clientId);
    map.insert(QStringLiteral("transport"), session.transport);
    map.insert(QStringLiteral("transportLabel"), session.transportLabel);
    map.insert(QStringLiteral("protocolVersion"), session.protocolVersion);
    map.insert(QStringLiteral("protocolVersionName"), session.protocolVersionName);
    map.insert(QStringLiteral("summary"), session.summary);
    map.insert(QStringLiteral("lastError"), session.lastError);
    map.insert(QStringLiteral("unreadMessageCount"), session.unreadMessageCount);
    return map;
}

void SessionListModel::setSessions(const QVector<SessionState> &sessions)
{
    QVector<SessionRow> rows;
    rows.reserve(sessions.size());
    for (const SessionState &session : sessions) {
        rows.append(rowFromSession(session));
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
                StateRole,
                ConnectedRole,
                HostRole,
                PortRole,
                ClientIdRole,
                TransportRole,
                TransportLabelRole,
                ProtocolVersionRole,
                ProtocolVersionNameRole,
                SummaryRole,
                LastErrorRole,
                UnreadMessageCountRole,
            });
    }
}

SessionListModel::SessionRow SessionListModel::rowFromSession(const SessionState &session)
{
    const QMqttClient *client = session.runtime.client;
    SessionRow row;
    row.id = session.id;
    row.name = session.name;
    row.state = sessionStateName(session, client);
    row.connected = client && client->state() == QMqttClient::Connected;
    row.host = client ? client->hostname() : QString();
    row.port = client ? client->port() : SessionConfig::kDefaultPort;
    row.clientId = client ? client->clientId() : QString();
    row.transport = session.transport;
    row.transportLabel = SessionConfig::transportLabel(session.transport);
    row.protocolVersion = session.protocolVersion;
    row.protocolVersionName = protocolVersionLabel(session.protocolVersion);
    row.summary = session.runtime.brokerInfo.isEmpty()
        ? session.runtime.lastError
        : session.runtime.brokerInfo;
    row.lastError = session.runtime.lastError;
    row.unreadMessageCount = (std::max)(
        qint64(0),
        session.runtime.totalMessageCount - session.runtime.viewedMessageCount);
    return row;
}
