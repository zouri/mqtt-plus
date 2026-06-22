#include "subscriptioncontroller.h"

#include "app/appfacadeutils.h"
#include "domain/sessionconfig.h"
#include "models/subscriptionlistmodel.h"
#include "services/payload/payloadcodec.h"

#include <QDateTime>

#include <algorithm>

using namespace AppFacadeUtils;

SubscriptionController::SubscriptionController(QObject *parent)
    : QObject(parent)
{
}

void SubscriptionController::setDependencies(Dependencies dependencies)
{
    m_dependencies = std::move(dependencies);
}

bool SubscriptionController::upsertCurrentSubscription(
    const QString &topic,
    int qos,
    int format,
    const QString &scriptId,
    const QString &alias)
{
    auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session) {
        return false;
    }

    const QString filter = topic.trimmed();
    if (filter.isEmpty()) {
        return false;
    }

    const QMqttTopicFilter topicFilter(filter);
    if (!topicFilter.isValid()) {
        if (m_dependencies.appendEvent) {
            m_dependencies.appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("Invalid topic filter: %1").arg(filter));
        }
        return false;
    }

    SubscriptionEntry *entry = subscriptionByTopic(session, filter);
    const QString sanitizedScriptId = m_dependencies.scriptExists && m_dependencies.scriptExists(scriptId) ? scriptId : QString();
    const QString displayAlias = alias.trimmed();
    if (!entry) {
        SubscriptionEntry subscription;
        subscription.topic = filter;
        subscription.alias = displayAlias;
        subscription.requestedQos = SessionConfig::sanitizeQos(qos);
        subscription.format = format;
        subscription.scriptId = sanitizedScriptId;
        session->subscriptions.append(subscription);
        entry = &session->subscriptions.last();
    } else {
        entry->alias = displayAlias;
        entry->requestedQos = SessionConfig::sanitizeQos(qos);
        entry->format = format;
        entry->scriptId = sanitizedScriptId;
        entry->paused = false;
        entry->lastError.clear();
    }

    session->subscriptionFormats.insert(filter, format);
    auto *client = session->client;
    if (client && client->state() == QMqttClient::Connected) {
        ensureSubscriptionActive(*session, *entry, true);
    }

    const bool saved = m_dependencies.saveSessions ? m_dependencies.saveSessions() : false;
    if (m_dependencies.notifySessionAndSubscriptionViewsChanged) {
        m_dependencies.notifySessionAndSubscriptionViewsChanged();
    }
    return saved;
}

bool SubscriptionController::updateCurrentSubscription(
    const QString &topic,
    const QString &newTopic,
    const QString &alias,
    const QString &scriptId)
{
    auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session) {
        return false;
    }

    const QString previousFilter = topic.trimmed();
    const QString filter = newTopic.trimmed();
    if (filter.isEmpty()) {
        return false;
    }

    const QMqttTopicFilter topicFilter(filter);
    if (!topicFilter.isValid()) {
        if (m_dependencies.appendEvent) {
            m_dependencies.appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("Invalid topic filter: %1").arg(filter));
        }
        return false;
    }

    SubscriptionEntry *entry = subscriptionByTopic(session, previousFilter);
    if (!entry) {
        return false;
    }

    if (filter != previousFilter && subscriptionByTopic(session, filter)) {
        if (m_dependencies.appendEvent) {
            m_dependencies.appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("%1 already exists").arg(filter));
        }
        return false;
    }

    const QString sanitizedScriptId = m_dependencies.scriptExists && m_dependencies.scriptExists(scriptId) ? scriptId : QString();
    const QString displayAlias = alias.trimmed();
    const bool topicChanged = entry->topic != filter;
    if (!topicChanged && entry->alias == displayAlias && entry->scriptId == sanitizedScriptId) {
        return true;
    }

    if (topicChanged) {
        if (entry->runtimeSubscription) {
            entry->runtimeSubscription->unsubscribe();
            entry->runtimeSubscription.clear();
        } else if (auto *client = session->client; client && client->state() == QMqttClient::Connected) {
            client->unsubscribe(QMqttTopicFilter(entry->topic));
        }

        session->subscriptionFormats.remove(entry->topic);
        entry->topic = filter;
        entry->grantedQos = -1;
        entry->runtimeState = entry->paused ? QStringLiteral("paused") : QStringLiteral("saved");
        entry->lastError.clear();
        entry->recentMessageTimestampsMs.clear();
        session->subscriptionFormats.insert(filter, entry->format);
    }

    entry->alias = displayAlias;
    entry->scriptId = sanitizedScriptId;

    auto *client = session->client;
    if (topicChanged && !entry->paused && client && client->state() == QMqttClient::Connected) {
        ensureSubscriptionActive(*session, *entry, true);
    }

    const bool saved = m_dependencies.saveSessions ? m_dependencies.saveSessions() : false;
    if (m_dependencies.notifyCurrentSessionAndSubscriptionsChanged) {
        m_dependencies.notifyCurrentSessionAndSubscriptionsChanged();
    }
    return saved;
}

void SubscriptionController::removeCurrentSubscription(const QString &topic)
{
    auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session) {
        return;
    }

    const QString filter = topic.trimmed();
    if (filter.isEmpty()) {
        return;
    }

    const auto it = std::find_if(
        session->subscriptions.begin(),
        session->subscriptions.end(),
        [&filter](const SubscriptionEntry &entry) { return entry.topic == filter; });
    if (it == session->subscriptions.end()) {
        return;
    }

    if (it->runtimeSubscription) {
        it->runtimeSubscription->unsubscribe();
    } else if (auto *client = session->client; client && client->state() == QMqttClient::Connected) {
        client->unsubscribe(QMqttTopicFilter(filter));
    }

    if (m_dependencies.appendEvent) {
        m_dependencies.appendEvent(*session, QStringLiteral("Subscription"), QStringLiteral("Removed %1").arg(filter));
    }
    session->subscriptionFormats.remove(filter);
    session->subscriptions.erase(it);
    if (m_dependencies.saveSessions) {
        m_dependencies.saveSessions();
    }
    if (m_dependencies.notifyCurrentSessionAndSubscriptionsChanged) {
        m_dependencies.notifyCurrentSessionAndSubscriptionsChanged();
    }
}

void SubscriptionController::setCurrentSubscriptionPaused(const QString &topic, bool paused)
{
    auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session) {
        return;
    }

    SubscriptionEntry *entry = subscriptionByTopic(session, topic.trimmed());
    if (!entry || entry->paused == paused) {
        return;
    }

    entry->paused = paused;
    if (paused) {
        entry->runtimeState = QStringLiteral("paused");
        if (entry->runtimeSubscription) {
            entry->runtimeSubscription->unsubscribe();
        } else if (auto *client = session->client; client && client->state() == QMqttClient::Connected) {
            client->unsubscribe(QMqttTopicFilter(entry->topic));
        }
        if (m_dependencies.appendEvent) {
            m_dependencies.appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("Paused %1").arg(entry->topic));
        }
    } else {
        entry->lastError.clear();
        if (entry->runtimeSubscription) {
            entry->runtimeSubscription->unsubscribe();
            entry->runtimeSubscription.clear();
        }
        auto *client = session->client;
        if (client && client->state() == QMqttClient::Connected) {
            ensureSubscriptionActive(*session, *entry, true);
        } else {
            if (m_dependencies.appendEvent) {
                m_dependencies.appendEvent(
                    *session,
                    QStringLiteral("Subscription"),
                    QStringLiteral("Queued %1 for reconnect").arg(entry->topic));
            }
        }
    }

    if (m_dependencies.saveSessions) {
        m_dependencies.saveSessions();
    }
    if (m_dependencies.notifyCurrentSessionAndSubscriptionsChanged) {
        m_dependencies.notifyCurrentSessionAndSubscriptionsChanged();
    }
}

SubscriptionEntry *SubscriptionController::subscriptionByTopic(SessionState *session, const QString &topic)
{
    if (!session) {
        return nullptr;
    }
    for (auto &entry : session->subscriptions) {
        if (entry.topic == topic) {
            return &entry;
        }
    }
    return nullptr;
}

const SubscriptionEntry *SubscriptionController::subscriptionByTopic(const SessionState *session, const QString &topic) const
{
    if (!session) {
        return nullptr;
    }
    for (const auto &entry : session->subscriptions) {
        if (entry.topic == topic) {
            return &entry;
        }
    }
    return nullptr;
}

const SubscriptionEntry *SubscriptionController::bestSubscriptionForTopic(
    const SessionState &session,
    const QString &topic) const
{
    const SubscriptionEntry *best = nullptr;
    int bestScore = -1;
    for (const auto &entry : session.subscriptions) {
        if (!PayloadCodec::topicFilterMatches(entry.topic, topic)) {
            continue;
        }
        const int score = topicSpecificityScore(entry.topic);
        if (score > bestScore || (score == bestScore && (!best || entry.topic < best->topic))) {
            bestScore = score;
            best = &entry;
        }
    }
    return best;
}

void SubscriptionController::restoreActiveSubscriptions(SessionState &session, bool emitEvents)
{
    for (auto &entry : session.subscriptions) {
        if (!entry.paused) {
            ensureSubscriptionActive(session, entry, emitEvents);
        }
    }
}

void SubscriptionController::resetRuntimeSubscriptions(SessionState &session)
{
    for (auto &entry : session.subscriptions) {
        entry.runtimeSubscription.clear();
        entry.runtimeState = entry.paused ? QStringLiteral("paused") : QStringLiteral("saved");
        entry.grantedQos = -1;
        entry.lastError.clear();
    }
}

void SubscriptionController::ensureSubscriptionActive(SessionState &session, SubscriptionEntry &entry, bool emitEvents)
{
    auto *client = session.client;
    if (entry.paused || !client || client->state() != QMqttClient::Connected) {
        return;
    }

    if (entry.runtimeSubscription) {
        const auto state = entry.runtimeSubscription->state();
        if (state == QMqttSubscription::Subscribed || state == QMqttSubscription::SubscriptionPending) {
            return;
        }
        entry.runtimeSubscription->unsubscribe();
        entry.runtimeSubscription.clear();
    }

    const QMqttTopicFilter filter(entry.topic);
    if (!filter.isValid()) {
        entry.runtimeState = QStringLiteral("error");
        entry.lastError = tr("Invalid topic filter.");
        if (emitEvents && m_dependencies.appendEvent) {
            m_dependencies.appendEvent(
                session,
                QStringLiteral("Subscription"),
                QStringLiteral("%1 is not a valid topic filter").arg(entry.topic));
        }
        return;
    }

    QMqttSubscription *subscription = client->subscribe(filter, SessionConfig::sanitizeQos(entry.requestedQos));
    if (!subscription) {
        entry.runtimeState = QStringLiteral("error");
        entry.lastError = tr("Qt MQTT returned no subscription object.");
        if (emitEvents && m_dependencies.appendEvent) {
            m_dependencies.appendEvent(
                session,
                QStringLiteral("Subscription"),
                QStringLiteral("Failed to subscribe to %1").arg(entry.topic));
        }
        return;
    }

    entry.runtimeSubscription = subscription;
    entry.runtimeState = subscriptionStateName(subscription->state());
    entry.grantedQos = subscription->qos();
    entry.lastError = subscription->reason();
    observeSubscription(session, entry, subscription);

    if (emitEvents && m_dependencies.appendEvent) {
        m_dependencies.appendEvent(
            session,
            QStringLiteral("Subscription"),
            QStringLiteral("Requested %1 at QoS %2").arg(entry.topic).arg(entry.requestedQos));
    }
}

void SubscriptionController::observeSubscription(SessionState &session, SubscriptionEntry &entry, QMqttSubscription *subscription)
{
    if (!subscription || subscription->property("mqttPlusObserved").toBool()) {
        return;
    }

    subscription->setProperty("mqttPlusObserved", true);
    connect(
        subscription,
        &QMqttSubscription::stateChanged,
        this,
        [this, sessionId = session.id, topic = entry.topic, subscription = QPointer<QMqttSubscription>(subscription)](
            QMqttSubscription::SubscriptionState state) {
            updateSubscriptionState(sessionId, topic, subscription, state);
        });
}

void SubscriptionController::updateSubscriptionState(
    const QString &sessionId,
    const QString &topic,
    const QPointer<QMqttSubscription> &subscription,
    QMqttSubscription::SubscriptionState state)
{
    auto *session = m_dependencies.sessionById ? m_dependencies.sessionById(sessionId) : nullptr;
    SubscriptionEntry *entry = subscriptionByTopic(session, topic);
    if (!session || !entry || entry->runtimeSubscription != subscription) {
        return;
    }

    const QString previousState = entry->runtimeState;
    entry->runtimeState = subscriptionStateName(state);
    if (entry->runtimeSubscription) {
        entry->grantedQos = entry->runtimeSubscription->qos();
        entry->lastError = entry->runtimeSubscription->reason();
    }

    if (state == QMqttSubscription::Subscribed) {
        entry->lastError.clear();
    }

    if (previousState != entry->runtimeState) {
        if (state == QMqttSubscription::Subscribed) {
            if (m_dependencies.appendEvent) {
                m_dependencies.appendEvent(
                    *session,
                    QStringLiteral("Subscription"),
                    QStringLiteral("Subscribed to %1").arg(entry->topic));
            }
        } else if (state == QMqttSubscription::Unsubscribed && entry->paused) {
            if (m_dependencies.appendEvent) {
                m_dependencies.appendEvent(
                    *session,
                    QStringLiteral("Subscription"),
                    QStringLiteral("Paused %1").arg(entry->topic));
            }
        } else if (state == QMqttSubscription::Error) {
            const QString reason = entry->lastError.isEmpty()
                ? QStringLiteral("Broker returned a subscription error.")
                : entry->lastError;
            if (m_dependencies.appendEvent) {
                m_dependencies.appendEvent(
                    *session,
                    QStringLiteral("Subscription"),
                    QStringLiteral("%1 failed: %2").arg(entry->topic).arg(reason));
            }
        }
    }

    if (m_dependencies.refreshSubscriptionsModel) {
        m_dependencies.refreshSubscriptionsModel();
    }
    if (m_dependencies.emitSubscriptionsChanged) {
        m_dependencies.emitSubscriptionsChanged();
    }
}

qreal SubscriptionController::subscriptionFps(const SubscriptionEntry &entry, qint64 nowMs) const
{
    return static_cast<qreal>(recentMessageCount(entry.recentMessageTimestampsMs, nowMs));
}

bool SubscriptionController::currentSessionHasActiveSubscriptionFps(qint64 nowMs) const
{
    const auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session) {
        return false;
    }

    for (const auto &subscription : session->subscriptions) {
        if (recentMessageCount(subscription.recentMessageTimestampsMs, nowMs) > 0) {
            return true;
        }
    }

    return false;
}

void SubscriptionController::refreshSubscriptionFps()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session) {
        if (m_dependencies.setSubscriptionFpsRefreshActive) {
            m_dependencies.setSubscriptionFpsRefreshActive(false);
        }
        if (m_dependencies.setTopicFpsRows) {
            m_dependencies.setTopicFpsRows({});
        }
        return;
    }

    bool hasActiveFps = false;
    QVector<SubscriptionFpsRow> rows;
    rows.reserve(session->subscriptions.size());
    for (const auto &subscription : session->subscriptions) {
        SubscriptionFpsRow row;
        row.topic = subscription.topic;
        row.topicFps = subscriptionFps(subscription, nowMs);
        hasActiveFps = hasActiveFps || row.topicFps > 0.0;
        rows.append(row);
    }

    if (!hasActiveFps) {
        if (m_dependencies.setSubscriptionFpsRefreshActive) {
            m_dependencies.setSubscriptionFpsRefreshActive(false);
        }
    }

    if (m_dependencies.setTopicFpsRows) {
        m_dependencies.setTopicFpsRows(rows);
    }
}
