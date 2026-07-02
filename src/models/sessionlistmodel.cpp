#include "sessionlistmodel.h"

#include "domain/session.h"
#include "domain/sessionconfig.h"
#include "services/apputils.h"

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
    const auto *client = session.client;

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
        return session.brokerInfo.isEmpty() ? session.lastError : session.brokerInfo;
    case LastErrorRole:
        return session.lastError;
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
    const auto *client = session.client;
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
    map.insert(QStringLiteral("summary"), session.brokerInfo.isEmpty() ? session.lastError : session.brokerInfo);
    map.insert(QStringLiteral("lastError"), session.lastError);
    return map;
}

void SessionListModel::setSource(const QVector<SessionState> *sessions)
{
    m_sessions = sessions;
    beginResetModel();
    endResetModel();
    emit countChanged();
}

void SessionListModel::notifyRefresh()
{
    beginResetModel();
    endResetModel();
    emit countChanged();
}
