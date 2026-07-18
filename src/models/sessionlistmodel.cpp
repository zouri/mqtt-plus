#include "sessionlistmodel.h"

#include "domain/session.h"
#include "domain/sessionconfig.h"
#include "services/apputils.h"

#include <algorithm>

using namespace AppUtils;

SessionListModel::SessionListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SessionListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !m_sessions ? 0 : m_sessions->size();
}

int SessionListModel::count() const
{
    return rowCount();
}

QVariant SessionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !m_sessions || index.row() < 0 || index.row() >= m_sessions->size()) {
        return {};
    }

    const auto &session = m_sessions->at(index.row());
    const auto *client = session.runtime.client;

    switch (role) {
    case IdRole:
        return session.id;
    case NameRole:
        return session.name;
    case StateRole:
        return sessionStateName(session, client);
    case ConnectedRole:
        return client && client->state() == QMqttClient::Connected;
    case HostRole:
        return client ? client->hostname() : QString();
    case PortRole:
        return client ? client->port() : SessionConfig::kDefaultPort;
    case ClientIdRole:
        return client ? client->clientId() : QString();
    case TransportRole:
        return session.transport;
    case TransportLabelRole:
        return transportLabel(session.transport);
    case ProtocolVersionRole:
        return session.protocolVersion;
    case ProtocolVersionNameRole:
        return protocolVersionLabel(session.protocolVersion);
    case SummaryRole:
        return session.runtime.brokerInfo.isEmpty() ? session.runtime.lastError : session.runtime.brokerInfo;
    case LastErrorRole:
        return session.runtime.lastError;
    case UnreadMessageCountRole:
        return (std::max)(qint64(0), session.runtime.totalMessageCount - session.runtime.viewedMessageCount);
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
    if (!m_sessions || row < 0 || row >= m_sessions->size()) {
        return {};
    }

    QVariantMap map;
    const auto &session = m_sessions->at(row);
    const auto *client = session.runtime.client;
    map.insert(QStringLiteral("id"), session.id);
    map.insert(QStringLiteral("name"), session.name);
    map.insert(QStringLiteral("state"), sessionStateName(session, client));
    map.insert(QStringLiteral("connected"), client && client->state() == QMqttClient::Connected);
    map.insert(QStringLiteral("host"), client ? client->hostname() : QString());
    map.insert(QStringLiteral("port"), client ? client->port() : SessionConfig::kDefaultPort);
    map.insert(QStringLiteral("clientId"), client ? client->clientId() : QString());
    map.insert(QStringLiteral("transport"), session.transport);
    map.insert(QStringLiteral("transportLabel"), transportLabel(session.transport));
    map.insert(QStringLiteral("protocolVersion"), session.protocolVersion);
    map.insert(QStringLiteral("protocolVersionName"), protocolVersionLabel(session.protocolVersion));
    map.insert(QStringLiteral("summary"), session.runtime.brokerInfo.isEmpty() ? session.runtime.lastError : session.runtime.brokerInfo);
    map.insert(QStringLiteral("lastError"), session.runtime.lastError);
    map.insert(
        QStringLiteral("unreadMessageCount"),
        (std::max)(qint64(0), session.runtime.totalMessageCount - session.runtime.viewedMessageCount));
    return map;
}

void SessionListModel::setSource(const QVector<SessionState> *sessions)
{
    m_sessions = sessions;
    beginResetModel();
    endResetModel();
    m_knownCount = count();
    emit countChanged();
}

void SessionListModel::notifyRefresh()
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
