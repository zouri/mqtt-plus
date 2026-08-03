#include "subscriptionlistmodel.h"

#include "domain/script.h"
#include "services/apputils.h"
#include "services/payload/payloadcodec.h"

#include <QDateTime>

#include <algorithm>
#include <utility>

using namespace AppUtils;

namespace {
bool hasPositiveRateSample(const QVariantList &history)
{
    return std::any_of(
        history.cbegin(),
        history.cend(),
        [](const QVariant &sample) { return sample.toReal() > 0.0; });
}
}

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

    const SubscriptionRow &row = m_rows.at(index.row());
    switch (role) {
    case TopicRole:
        return row.topic;
    case AliasRole:
        return row.alias;
    case DisplayNameRole:
        return displayName(row);
    case RequestedQosRole:
        return row.requestedQos;
    case GrantedQosRole:
        return row.grantedQos;
    case TopicFpsRole:
        return row.topicFps;
    case TopicRateHistoryRole:
        return row.topicRateHistory;
    case FormatRole:
        return row.format;
    case FormatNameRole:
        return PayloadCodec::formatName(PayloadCodec::formatFromInt(row.format));
    case ScriptIdRole:
        return row.scriptId;
    case ScriptNameRole:
        return row.scriptName;
    case ColorRole:
        return row.color;
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
    static const QHash<int, QByteArray> roles = {
        {TopicRole, "topic"},
        {AliasRole, "alias"},
        {DisplayNameRole, "displayName"},
        {RequestedQosRole, "requestedQos"},
        {GrantedQosRole, "grantedQos"},
        {TopicFpsRole, "topicFps"},
        {TopicRateHistoryRole, "topicRateHistory"},
        {FormatRole, "format"},
        {FormatNameRole, "formatName"},
        {ScriptIdRole, "scriptId"},
        {ScriptNameRole, "scriptName"},
        {ColorRole, "topicColor"},
        {PausedRole, "paused"},
        {StateRole, "subscriptionState"},
        {LastErrorRole, "lastError"},
    };
    return roles;
}

QVariantMap SubscriptionListModel::rowAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }

    return rowToMap(m_rows.at(row));
}

void SubscriptionListModel::setSubscriptions(
    const QString &sourceSessionId,
    const QVector<SubscriptionEntry> &subscriptions,
    const QVector<ScriptEntry> &scripts)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QVector<SubscriptionRow> rows;
    rows.reserve(subscriptions.size());
    for (const SubscriptionEntry &subscription : subscriptions) {
        rows.append(rowFromSubscription(subscription, scripts, nowMs));
    }

    const bool sourceChanged = sourceSessionId != m_sourceSessionId;
    const bool countWillChange = rows.size() != m_rows.size();
    m_sourceSessionId = sourceSessionId;
    if (sourceChanged || countWillChange) {
        beginResetModel();
        m_rows = std::move(rows);
        endResetModel();
        m_lastRateHistorySampleMs = 0;
        if (countWillChange) {
            emit countChanged();
        }
        return;
    }

    for (qsizetype i = 0; i < rows.size(); ++i) {
        if (!rows[i].paused
            && rows[i].topic == m_rows.at(i).topic) {
            rows[i].topicRateHistory = m_rows.at(i).topicRateHistory;
        }
    }
    for (qsizetype row = 0; row < rows.size(); ++row) {
        const QList<int> roles = changedRoles(m_rows.at(row), rows.at(row));
        if (roles.isEmpty()) {
            continue;
        }
        m_rows[row] = std::move(rows[row]);
        emit dataChanged(index(static_cast<int>(row), 0), index(static_cast<int>(row), 0), roles);
    }
}

bool SubscriptionListModel::updateTopicFps(
    const QVector<SubscriptionEntry> &subscriptions,
    qint64 nowMs)
{
    if (subscriptions.size() != m_rows.size() || m_rows.isEmpty()) {
        return false;
    }

    const bool historySampleDue = m_lastRateHistorySampleMs <= 0
        || nowMs - m_lastRateHistorySampleMs >= kSubscriptionRateHistorySampleIntervalMs;
    bool sampledHistory = false;
    bool hasRateActivity = false;
    bool hasRateHistory = false;
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        const SubscriptionEntry &subscription = subscriptions.at(i);
        SubscriptionRow &row = m_rows[i];
        const qreal previousFps = row.topicFps;
        const QVariantList previousHistory = row.topicRateHistory;
        if (row.topic != subscription.topic) {
            row.topicFps = 0.0;
            row.topicRateHistory.clear();
        } else {
            QVariantList &history = row.topicRateHistory;
            if (subscription.paused) {
                row.topicFps = 0.0;
                history.clear();
            } else {
                row.topicFps = static_cast<qreal>(
                    recentMessageCount(subscription.recentMessageTimestampsMs, nowMs));
                const qreal currentRate = row.topicFps;
                if (history.isEmpty() && currentRate > 0.0) {
                    history.reserve(kSubscriptionRateHistorySampleCount);
                    for (int sample = 1; sample < kSubscriptionRateHistorySampleCount; ++sample) {
                        history.append(0.0);
                    }
                    history.append(currentRate);
                    sampledHistory = true;
                } else if (!history.isEmpty() && historySampleDue) {
                    history.append(currentRate);
                    while (history.size() > kSubscriptionRateHistorySampleCount) {
                        history.removeFirst();
                    }
                    if (!hasPositiveRateSample(history)) {
                        history.clear();
                    }
                    sampledHistory = true;
                }
            }
        }

        QList<int> roles;
        if (previousFps != row.topicFps) {
            roles.append(TopicFpsRole);
        }
        if (previousHistory != row.topicRateHistory) {
            roles.append(TopicRateHistoryRole);
        }
        if (!roles.isEmpty()) {
            emit dataChanged(index(static_cast<int>(i), 0), index(static_cast<int>(i), 0), roles);
        }

        hasRateActivity = hasRateActivity || row.topicFps > 0.0;
        hasRateHistory = hasRateHistory || !row.topicRateHistory.isEmpty();
    }

    if (historySampleDue && sampledHistory) {
        m_lastRateHistorySampleMs = nowMs;
    }
    if (!hasRateHistory) {
        m_lastRateHistorySampleMs = 0;
    }

    return hasRateActivity || hasRateHistory;
}

SubscriptionListModel::SubscriptionRow SubscriptionListModel::rowFromSubscription(
    const SubscriptionEntry &subscription,
    const QVector<ScriptEntry> &scripts,
    qint64 nowMs)
{
    QString scriptName;
    for (const ScriptEntry &script : scripts) {
        if (script.id == subscription.scriptId) {
            scriptName = script.name;
            break;
        }
    }

    return {
        subscription.topic,
        subscription.alias,
        subscription.requestedQos,
        subscription.grantedQos,
        subscription.paused
            ? 0.0
            : static_cast<qreal>(recentMessageCount(subscription.recentMessageTimestampsMs, nowMs)),
        {},
        subscription.format,
        subscription.scriptId,
        scriptName,
        subscription.color,
        subscription.paused,
        subscription.runtimeState,
        subscription.lastError,
    };
}

QVariantMap SubscriptionListModel::rowToMap(const SubscriptionRow &row)
{
    QVariantMap map;
    map.insert(QStringLiteral("topic"), row.topic);
    map.insert(QStringLiteral("alias"), row.alias);
    map.insert(QStringLiteral("displayName"), displayName(row));
    map.insert(QStringLiteral("requestedQos"), row.requestedQos);
    map.insert(QStringLiteral("grantedQos"), row.grantedQos);
    map.insert(QStringLiteral("topicFps"), row.topicFps);
    map.insert(QStringLiteral("topicRateHistory"), row.topicRateHistory);
    map.insert(QStringLiteral("format"), row.format);
    map.insert(QStringLiteral("formatName"), PayloadCodec::formatName(PayloadCodec::formatFromInt(row.format)));
    map.insert(QStringLiteral("scriptId"), row.scriptId);
    map.insert(QStringLiteral("scriptName"), row.scriptName);
    map.insert(QStringLiteral("color"), row.color);
    map.insert(QStringLiteral("paused"), row.paused);
    map.insert(QStringLiteral("state"), row.state);
    map.insert(QStringLiteral("lastError"), row.lastError);
    return map;
}

QString SubscriptionListModel::displayName(const SubscriptionRow &row)
{
    return row.alias.isEmpty() ? row.topic : row.alias;
}

QList<int> SubscriptionListModel::changedRoles(
    const SubscriptionRow &before,
    const SubscriptionRow &after)
{
    QList<int> roles;
    if (before.topic != after.topic) {
        roles.append(TopicRole);
    }
    if (before.alias != after.alias) {
        roles.append(AliasRole);
    }
    if (before.topic != after.topic || before.alias != after.alias) {
        roles.append(DisplayNameRole);
    }
    if (before.requestedQos != after.requestedQos) {
        roles.append(RequestedQosRole);
    }
    if (before.grantedQos != after.grantedQos) {
        roles.append(GrantedQosRole);
    }
    if (before.topicFps != after.topicFps) {
        roles.append(TopicFpsRole);
    }
    if (before.topicRateHistory != after.topicRateHistory) {
        roles.append(TopicRateHistoryRole);
    }
    if (before.format != after.format) {
        roles.append(FormatRole);
        roles.append(FormatNameRole);
    }
    if (before.scriptId != after.scriptId) {
        roles.append(ScriptIdRole);
    }
    if (before.scriptName != after.scriptName) {
        roles.append(ScriptNameRole);
    }
    if (before.color != after.color) {
        roles.append(ColorRole);
    }
    if (before.paused != after.paused) {
        roles.append(PausedRole);
    }
    if (before.state != after.state) {
        roles.append(StateRole);
    }
    if (before.lastError != after.lastError) {
        roles.append(LastErrorRole);
    }
    return roles;
}
