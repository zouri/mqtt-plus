#include "subscriptionlistmodel.h"

#include "domain/session.h"
#include "services/apputils.h"
#include "services/payload/payloadcodec.h"

#include <QDateTime>

using namespace AppUtils;

SubscriptionListModel::SubscriptionListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SubscriptionListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !m_subs ? 0 : m_subs->size();
}

int SubscriptionListModel::count() const
{
    return rowCount();
}

QVariant SubscriptionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !m_subs || index.row() < 0 || index.row() >= m_subs->size()) {
        return {};
    }

    const auto &sub = m_subs->at(index.row());
    const int row = index.row();
    switch (role) {
    case TopicRole:
        return sub.topic;
    case AliasRole:
        return sub.alias;
    case DisplayNameRole:
        return displayNameForSub(sub);
    case RequestedQosRole:
        return sub.requestedQos;
    case GrantedQosRole:
        return sub.grantedQos;
    case TopicFpsRole:
        return row < m_fpsCache.size() ? m_fpsCache.at(row) : 0.0;
    case FormatRole:
        return sub.format;
    case FormatNameRole:
        return PayloadCodec::formatName(PayloadCodec::formatFromInt(sub.format));
    case ScriptIdRole:
        return sub.scriptId;
    case ScriptNameRole:
        return row < m_scriptNameCache.size() ? m_scriptNameCache.at(row) : QString();
    case PausedRole:
        return sub.paused;
    case StateRole:
        return sub.runtimeState;
    case LastErrorRole:
        return sub.lastError;
    default:
        return {};
    }
}

QHash<int, QByteArray> SubscriptionListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
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
    return roles;
}

QVariantMap SubscriptionListModel::rowAt(int row) const
{
    if (!m_subs || row < 0 || row >= m_subs->size()) {
        return {};
    }

    return rowToMap(m_subs->at(row), row);
}

void SubscriptionListModel::setSource(const SessionState *session)
{
    if (session) {
        m_subs = &session->subscriptions;
    } else {
        m_subs = &m_empty;
    }
    rebuildCache();
}

void SubscriptionListModel::setScriptNameLookup(std::function<QString(const QString &)> lookup)
{
    m_scriptNameLookup = std::move(lookup);
}

void SubscriptionListModel::notifyRefresh()
{
    rebuildCache();
}

void SubscriptionListModel::updateTopicFps(qint64 nowMs)
{
    if (!m_subs || m_subs->isEmpty()) {
        return;
    }

    const qsizetype n = m_subs->size();
    m_fpsCache.resize(n);
    for (qsizetype i = 0; i < n; ++i) {
        m_fpsCache[i] = static_cast<qreal>(recentMessageCount(m_subs->at(i).recentMessageTimestampsMs, nowMs));
    }

    for (qsizetype i = 0; i < n; ++i) {
        const QModelIndex rowIndex = index(static_cast<int>(i), 0);
        emit dataChanged(rowIndex, rowIndex, {TopicFpsRole});
    }
}

void SubscriptionListModel::rebuildCache()
{
    const bool countWillChange = !m_subs || m_fpsCache.size() != static_cast<qsizetype>(m_subs->size());
    const auto rebuildRows = [this]() {
        if (!m_subs) {
            m_fpsCache.clear();
            m_scriptNameCache.clear();
            return;
        }

        const qsizetype rowCount = m_subs->size();
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        m_fpsCache.resize(rowCount);

        m_scriptNameCache.resize(rowCount);
        for (qsizetype i = 0; i < rowCount; ++i) {
            m_fpsCache[i] = static_cast<qreal>(recentMessageCount(m_subs->at(i).recentMessageTimestampsMs, nowMs));
            m_scriptNameCache[i] = m_scriptNameLookup
                ? m_scriptNameLookup(m_subs->at(i).scriptId)
                : QString();
        }
    };

    if (countWillChange) {
        beginResetModel();
        rebuildRows();
        endResetModel();
        emit countChanged();
        return;
    }

    rebuildRows();
    if (m_subs && !m_subs->isEmpty()) {
        emit dataChanged(
            index(0, 0),
            index(static_cast<int>(m_subs->size() - 1), 0),
            {
                TopicRole,
                AliasRole,
                DisplayNameRole,
                RequestedQosRole,
                GrantedQosRole,
                TopicFpsRole,
                FormatRole,
                FormatNameRole,
                ScriptIdRole,
                ScriptNameRole,
                PausedRole,
                StateRole,
                LastErrorRole,
            });
    }
}

QVariantMap SubscriptionListModel::rowToMap(const SubscriptionEntry &sub, int row) const
{
    QVariantMap map;
    const qreal fps = row >= 0 && row < m_fpsCache.size() ? m_fpsCache.at(row) : 0.0;
    const QString scriptName = row >= 0 && row < m_scriptNameCache.size() ? m_scriptNameCache.at(row) : QString();
    map.insert(QStringLiteral("topic"), sub.topic);
    map.insert(QStringLiteral("alias"), sub.alias);
    map.insert(QStringLiteral("displayName"), displayNameForSub(sub));
    map.insert(QStringLiteral("requestedQos"), sub.requestedQos);
    map.insert(QStringLiteral("grantedQos"), sub.grantedQos);
    map.insert(QStringLiteral("topicFps"), fps);
    map.insert(QStringLiteral("format"), sub.format);
    map.insert(QStringLiteral("formatName"), PayloadCodec::formatName(PayloadCodec::formatFromInt(sub.format)));
    map.insert(QStringLiteral("scriptId"), sub.scriptId);
    map.insert(QStringLiteral("scriptName"), scriptName);
    map.insert(QStringLiteral("paused"), sub.paused);
    map.insert(QStringLiteral("state"), sub.runtimeState);
    map.insert(QStringLiteral("lastError"), sub.lastError);
    return map;
}

QString SubscriptionListModel::displayNameForSub(const SubscriptionEntry &sub) const
{
    return sub.alias.isEmpty() ? sub.topic : sub.alias;
}
