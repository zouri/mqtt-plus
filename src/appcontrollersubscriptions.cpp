#include "appcontroller.h"

#include "appcontrollerutils.h"
#include "sessionconfig.h"

#include <algorithm>

using namespace AppControllerUtils;

bool AppController::upsertCurrentSubscription(
    const QString &topic,
    int qos,
    int format,
    const QString &scriptId,
    const QString &alias)
{
    auto *session = currentSessionState();
    if (!session) {
        return false;
    }

    const QString filter = topic.trimmed();
    if (filter.isEmpty()) {
        return false;
    }

    const QMqttTopicFilter topicFilter(filter);
    if (!topicFilter.isValid()) {
        appendEvent(*session, QStringLiteral("Subscription"), QStringLiteral("Invalid topic filter: %1").arg(filter));
        return false;
    }

    SubscriptionEntry *entry = subscriptionByTopic(session, filter);
    const QString sanitizedScriptId = scriptById(scriptId) ? scriptId : QString();
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
    if (session->client->state() == QMqttClient::Connected) {
        ensureSubscriptionActive(*session, *entry, true);
    }

    saveSessions();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit sessionsChanged();
    return true;
}

bool AppController::updateCurrentSubscription(const QString &topic, const QString &alias, const QString &scriptId)
{
    auto *session = currentSessionState();
    if (!session) {
        return false;
    }

    SubscriptionEntry *entry = subscriptionByTopic(session, topic.trimmed());
    if (!entry) {
        return false;
    }

    const QString sanitizedScriptId = scriptById(scriptId) ? scriptId : QString();
    const QString displayAlias = alias.trimmed();
    if (entry->alias == displayAlias && entry->scriptId == sanitizedScriptId) {
        return true;
    }

    entry->alias = displayAlias;
    entry->scriptId = sanitizedScriptId;
    saveSessions();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    return true;
}

void AppController::removeCurrentSubscription(const QString &topic)
{
    auto *session = currentSessionState();
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
    } else if (session->client->state() == QMqttClient::Connected) {
        session->client->unsubscribe(QMqttTopicFilter(filter));
    }

    appendEvent(*session, QStringLiteral("Subscription"), QStringLiteral("Removed %1").arg(filter));
    session->subscriptionFormats.remove(filter);
    session->subscriptions.erase(it);
    saveSessions();
    emit currentSessionChanged();
    emit subscriptionsChanged();
}

void AppController::setCurrentSubscriptionPaused(const QString &topic, bool paused)
{
    auto *session = currentSessionState();
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
        } else if (session->client->state() == QMqttClient::Connected) {
            session->client->unsubscribe(QMqttTopicFilter(entry->topic));
        }
        appendEvent(*session, QStringLiteral("Subscription"), QStringLiteral("Paused %1").arg(entry->topic));
    } else {
        entry->lastError.clear();
        if (session->client->state() == QMqttClient::Connected) {
            ensureSubscriptionActive(*session, *entry, true);
        } else {
            appendEvent(*session, QStringLiteral("Subscription"), QStringLiteral("Queued %1 for reconnect").arg(entry->topic));
        }
    }

    saveSessions();
    emit currentSessionChanged();
    emit subscriptionsChanged();
}

AppController::SubscriptionEntry *AppController::subscriptionByTopic(SessionState *session, const QString &topic)
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

const AppController::SubscriptionEntry *AppController::subscriptionByTopic(const SessionState *session, const QString &topic) const
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

const AppController::SubscriptionEntry *AppController::bestSubscriptionForTopic(
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

void AppController::restoreActiveSubscriptions(SessionState &session, bool emitEvents)
{
    for (auto &entry : session.subscriptions) {
        if (!entry.paused) {
            ensureSubscriptionActive(session, entry, emitEvents);
        }
    }
}

void AppController::ensureSubscriptionActive(SessionState &session, SubscriptionEntry &entry, bool emitEvents)
{
    if (entry.paused || !session.client || session.client->state() != QMqttClient::Connected) {
        return;
    }

    const QMqttTopicFilter filter(entry.topic);
    if (!filter.isValid()) {
        entry.runtimeState = QStringLiteral("error");
        entry.lastError = QStringLiteral("Invalid topic filter.");
        if (emitEvents) {
            appendEvent(session, QStringLiteral("Subscription"), QStringLiteral("%1 is not a valid topic filter").arg(entry.topic));
        }
        return;
    }

    QMqttSubscription *subscription = session.client->subscribe(filter, SessionConfig::sanitizeQos(entry.requestedQos));
    if (!subscription) {
        entry.runtimeState = QStringLiteral("error");
        entry.lastError = QStringLiteral("Qt MQTT returned no subscription object.");
        if (emitEvents) {
            appendEvent(session, QStringLiteral("Subscription"), QStringLiteral("Failed to subscribe to %1").arg(entry.topic));
        }
        return;
    }

    entry.runtimeSubscription = subscription;
    entry.runtimeState = subscriptionStateName(subscription->state());
    entry.grantedQos = subscription->qos();
    entry.lastError = subscription->reason();
    observeSubscription(session, entry, subscription);

    if (emitEvents) {
        appendEvent(
            session,
            QStringLiteral("Subscription"),
            QStringLiteral("Requested %1 at QoS %2").arg(entry.topic).arg(entry.requestedQos));
    }
}

void AppController::observeSubscription(SessionState &session, SubscriptionEntry &entry, QMqttSubscription *subscription)
{
    if (!subscription || subscription->property("mqttPlusObserved").toBool()) {
        return;
    }

    subscription->setProperty("mqttPlusObserved", true);
    connect(
        subscription,
        &QMqttSubscription::stateChanged,
        this,
        [this, sessionId = session.id, topic = entry.topic](QMqttSubscription::SubscriptionState state) {
            updateSubscriptionState(sessionId, topic, state);
        });
}

void AppController::updateSubscriptionState(
    const QString &sessionId,
    const QString &topic,
    QMqttSubscription::SubscriptionState state)
{
    auto *session = sessionById(sessionId);
    SubscriptionEntry *entry = subscriptionByTopic(session, topic);
    if (!session || !entry) {
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
            appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("Subscribed to %1").arg(entry->topic));
        } else if (state == QMqttSubscription::Unsubscribed && entry->paused) {
            appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("Paused %1").arg(entry->topic));
        } else if (state == QMqttSubscription::Error) {
            const QString reason = entry->lastError.isEmpty()
                ? QStringLiteral("Broker returned a subscription error.")
                : entry->lastError;
            appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("%1 failed: %2").arg(entry->topic, reason));
        }
    }

    emit subscriptionsChanged();
}

qreal AppController::subscriptionFps(const SubscriptionEntry &entry, qint64 nowMs) const
{
    return static_cast<qreal>(recentMessageCount(entry.recentMessageTimestampsMs, nowMs));
}

bool AppController::currentSessionHasActiveSubscriptionFps(qint64 nowMs) const
{
    const auto *session = currentSessionState();
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

void AppController::refreshSubscriptionFps()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!currentSessionHasActiveSubscriptionFps(nowMs)) {
        m_subscriptionFpsRefreshTimer.stop();
    }

    emit subscriptionsChanged();
}

