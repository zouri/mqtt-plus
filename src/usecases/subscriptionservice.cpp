#include "subscriptionservice.h"

#include "usecases/eventhistoryservice.h"
#include "usecases/sessionservice.h"
#include "services/apputils.h"
#include "services/mqtt/qtmqttpropertycodec.h"
#include "domain/sessionconfig.h"
#include <QRegularExpression>
#include <QSet>
#include <QTimer>

#include <algorithm>

using namespace AppUtils;

namespace {
QString sanitizeTopicColor(const QString &color)
{
    const QString trimmed = color.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    static const QRegularExpression hexColorPattern(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    return hexColorPattern.match(trimmed).hasMatch() ? trimmed.toUpper() : QString();
}

void reloadCurrentMessagesIfNeeded(
    EventHistoryService &eventHistoryService,
    SessionService &sessionService,
    const SessionState *session)
{
    if (session && session == sessionService.currentSession()) {
        eventHistoryService.reloadCurrentSessionHistory();
    }
}

ProcessorReference normalizedProcessorReference(const ProcessorReference &source)
{
    ProcessorReference result = source;
    result.processorId = result.processorId.trimmed();
    if (result.processorId.isEmpty()) {
        return {};
    }
    return result;
}
}

SubscriptionService::SubscriptionService(
    SessionService &sessionService,
    EventHistoryService &eventHistoryService,
    QObject *parent)
    : QObject(parent)
    , m_sessionService(sessionService)
    , m_eventHistoryService(eventHistoryService)
{
    connect(
        this,
        &SubscriptionService::subscriptionsChanged,
        &m_eventHistoryService,
        &EventHistoryService::invalidateMessageContexts);
}

bool SubscriptionService::upsertCurrentSubscription(
    const QString &topic,
    int qos,
    int format,
    const ProcessorReference &processor,
    const QString &color,
    const QString &alias,
    const MqttSubscriptionOptions &options)
{
    return upsertCurrentSubscriptions({topic}, qos, format, processor, color, alias, options);
}

bool SubscriptionService::upsertCurrentSubscriptions(
    const QStringList &topics,
    int qos,
    int format,
    const ProcessorReference &processor,
    const QString &color,
    const QString &alias,
    const MqttSubscriptionOptions &options)
{
    auto *session = m_sessionService.currentSession();
    if (!session) {
        return false;
    }

    QStringList filters;
    QSet<QString> seen;
    for (const QString &topic : topics) {
        const QString filter = topic.trimmed();
        if (!filter.isEmpty() && !seen.contains(filter)) {
            filters.append(filter);
            seen.insert(filter);
        }
    }
    if (filters.isEmpty()) {
        return false;
    }

    for (const QString &filter : filters) {
        const QMqttTopicFilter topicFilter(filter);
        if (!topicFilter.isValid()) {
            m_eventHistoryService.appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("Invalid topic filter: %1").arg(filter));
            return false;
        }
    }

    const ProcessorReference normalizedProcessor = normalizedProcessorReference(processor);
    const QString sanitizedColor = sanitizeTopicColor(color);
    const QString displayAlias = alias.trimmed();
    bool shouldReloadMessages = false;
    for (const QString &filter : filters) {
        SubscriptionEntry *entry = subscriptionByTopic(session, filter);
        if (!entry) {
            SubscriptionEntry subscription;
            subscription.topic = filter;
            subscription.alias = displayAlias;
            subscription.requestedQos = SessionConfig::sanitizeQos(qos);
            subscription.format = format;
            subscription.options = options;
            subscription.processor = normalizedProcessor;
            subscription.color = sanitizedColor;
            session->subscriptions.append(subscription);
            entry = &session->subscriptions.last();
            shouldReloadMessages = shouldReloadMessages || !sanitizedColor.isEmpty();
        } else {
            shouldReloadMessages = shouldReloadMessages || entry->color != sanitizedColor;
            entry->alias = displayAlias;
            entry->requestedQos = SessionConfig::sanitizeQos(qos);
            entry->format = format;
            entry->options = options;
            entry->processor = normalizedProcessor;
            entry->color = sanitizedColor;
            entry->paused = false;
            entry->lastError.clear();
        }

        session->runtime.subscriptionFormats.insert(filter, format);
        auto *client = session->runtime.client;
        if (client && client->state() == QMqttClient::Connected) {
            ensureSubscriptionActive(*session, *entry, true);
        }
    }

    const bool saved = m_sessionService.saveSessions();
    emit subscriptionsChanged();
    if (shouldReloadMessages) {
        reloadCurrentMessagesIfNeeded(m_eventHistoryService, m_sessionService, session);
    }
    return saved;
}

bool SubscriptionService::updateCurrentSubscription(
    const QString &topic,
    const QString &newTopic,
    const QString &alias,
    int qos,
    int format,
    const ProcessorReference &processor,
    const QString &color,
    const MqttSubscriptionOptions &options)
{
    auto *session = m_sessionService.currentSession();
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
        m_eventHistoryService.appendEvent(*session, QStringLiteral("Subscription"), QStringLiteral("Invalid topic filter: %1").arg(filter));
        return false;
    }

    SubscriptionEntry *entry = subscriptionByTopic(session, previousFilter);
    if (!entry) {
        return false;
    }

    if (filter != previousFilter && subscriptionByTopic(session, filter)) {
        m_eventHistoryService.appendEvent(
            *session,
            QStringLiteral("Subscription"),
            QStringLiteral("%1 already exists").arg(filter));
        return false;
    }

    const ProcessorReference normalizedProcessor = normalizedProcessorReference(processor);
    const QString sanitizedColor = sanitizeTopicColor(color);
    const QString displayAlias = alias.trimmed();
    const int sanitizedQos = SessionConfig::sanitizeQos(qos);
    const int sanitizedFormat = (std::max)(0, format);
    const bool topicChanged = entry->topic != filter;
    const bool colorChanged = entry->color != sanitizedColor;
    const bool qosChanged = entry->requestedQos != sanitizedQos;
    const bool formatChanged = entry->format != sanitizedFormat;
    const bool optionsChanged = entry->options != options;
    if (!topicChanged
        && !colorChanged
        && entry->alias == displayAlias
        && entry->requestedQos == sanitizedQos
        && entry->format == sanitizedFormat
        && !optionsChanged
        && entry->processor.processorId == normalizedProcessor.processorId
        && entry->processor.parameters == normalizedProcessor.parameters) {
        return true;
    }

    const bool shouldResubscribe = topicChanged || qosChanged || optionsChanged;
    auto *client = session->runtime.client;
    if (shouldResubscribe) {
        if (entry->runtimeSubscription) {
            entry->runtimeSubscription->unsubscribe();
            entry->runtimeSubscription.clear();
        } else if (client && client->state() == QMqttClient::Connected) {
            client->unsubscribe(QMqttTopicFilter(entry->topic));
        }
    }

    if (topicChanged) {
        session->runtime.subscriptionFormats.remove(entry->topic);
        entry->topic = filter;
        entry->recentMessages.clear();
    }

    if (shouldResubscribe) {
        entry->grantedQos = -1;
        entry->runtimeState = entry->paused ? QStringLiteral("paused") : QStringLiteral("saved");
        entry->lastError.clear();
    }

    entry->alias = displayAlias;
    entry->requestedQos = sanitizedQos;
    entry->format = sanitizedFormat;
    entry->options = options;
    entry->processor = normalizedProcessor;
    entry->color = sanitizedColor;
    session->runtime.subscriptionFormats.insert(entry->topic, entry->format);

    if (shouldResubscribe && !entry->paused && client && client->state() == QMqttClient::Connected) {
        ensureSubscriptionActive(*session, *entry, true);
    }

    const bool saved = m_sessionService.saveSessions();
    emit subscriptionsChanged();
    if (topicChanged || colorChanged || formatChanged) {
        reloadCurrentMessagesIfNeeded(m_eventHistoryService, m_sessionService, session);
    }
    return saved;
}

void SubscriptionService::removeCurrentSubscription(const QString &topic)
{
    auto *session = m_sessionService.currentSession();
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
    } else if (auto *client = session->runtime.client; client && client->state() == QMqttClient::Connected) {
        client->unsubscribe(QMqttTopicFilter(filter));
    }

    m_eventHistoryService.appendEvent(*session, QStringLiteral("Subscription"), QStringLiteral("Removed %1").arg(filter));
    session->runtime.subscriptionFormats.remove(filter);
    session->subscriptions.erase(it);
    m_sessionService.saveSessions();
    emit subscriptionsChanged();
}

void SubscriptionService::setCurrentSubscriptionPaused(const QString &topic, bool paused)
{
    auto *session = m_sessionService.currentSession();
    if (!session) {
        return;
    }

    SubscriptionEntry *entry = subscriptionByTopic(session, topic.trimmed());
    if (!entry || entry->paused == paused) {
        return;
    }

    entry->paused = paused;
    entry->recentMessages.clear();
    if (paused) {
        entry->runtimeState = QStringLiteral("paused");
        if (entry->runtimeSubscription) {
            entry->runtimeSubscription->unsubscribe();
        } else if (auto *client = session->runtime.client; client && client->state() == QMqttClient::Connected) {
            client->unsubscribe(QMqttTopicFilter(entry->topic));
        }
        m_eventHistoryService.appendEvent(*session, QStringLiteral("Subscription"), QStringLiteral("Paused %1").arg(entry->topic));
    } else {
        entry->lastError.clear();
        if (entry->runtimeSubscription) {
            entry->runtimeSubscription->unsubscribe();
            entry->runtimeSubscription.clear();
        }
        auto *client = session->runtime.client;
        if (client && client->state() == QMqttClient::Connected) {
            ensureSubscriptionActive(*session, *entry, true);
        } else {
            m_eventHistoryService.appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("Queued %1 for reconnect").arg(entry->topic));
        }
    }

    m_sessionService.saveSessions();
    emit subscriptionsChanged();
}

void SubscriptionService::setAllCurrentSubscriptionsPaused(bool paused)
{
    auto *session = m_sessionService.currentSession();
    if (!session) {
        return;
    }

    bool changed = false;
    for (auto &entry : session->subscriptions) {
        if (entry.paused == paused) {
            continue;
        }

        changed = true;
        entry.paused = paused;
        entry.recentMessages.clear();
        if (paused) {
            entry.runtimeState = QStringLiteral("paused");
            if (entry.runtimeSubscription) {
                entry.runtimeSubscription->unsubscribe();
            } else if (auto *client = session->runtime.client; client && client->state() == QMqttClient::Connected) {
                client->unsubscribe(QMqttTopicFilter(entry.topic));
            }
        } else {
            entry.lastError.clear();
            if (entry.runtimeSubscription) {
                entry.runtimeSubscription->unsubscribe();
                entry.runtimeSubscription.clear();
            }
            if (auto *client = session->runtime.client; client && client->state() == QMqttClient::Connected) {
                ensureSubscriptionActive(*session, entry, false);
            } else {
                entry.runtimeState = QStringLiteral("saved");
            }
        }
    }

    if (!changed) {
        return;
    }

    m_eventHistoryService.appendEvent(
        *session,
        QStringLiteral("Subscription"),
        paused ? QStringLiteral("Paused all subscriptions") : QStringLiteral("Resumed all subscriptions"));
    m_sessionService.saveSessions();
    emit subscriptionsChanged();
}

SubscriptionEntry *SubscriptionService::subscriptionByTopic(SessionState *session, const QString &topic)
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

void SubscriptionService::restoreActiveSubscriptions(SessionState &session, bool emitEvents)
{
    for (auto &entry : session.subscriptions) {
        if (!entry.paused) {
            ensureSubscriptionActive(session, entry, emitEvents);
        }
    }
}

void SubscriptionService::resetRuntimeSubscriptions(SessionState &session)
{
    for (auto &entry : session.subscriptions) {
        entry.runtimeSubscription.clear();
        entry.runtimeState = entry.paused ? QStringLiteral("paused") : QStringLiteral("saved");
        entry.grantedQos = -1;
        entry.lastError.clear();
    }
}

void SubscriptionService::ensureSubscriptionActive(SessionState &session, SubscriptionEntry &entry, bool emitEvents)
{
    auto *client = session.runtime.client;
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
        if (emitEvents) {
            m_eventHistoryService.appendEvent(
                session,
                QStringLiteral("Subscription"),
                QStringLiteral("%1 is not a valid topic filter").arg(entry.topic));
        }
        return;
    }

    QMqttSubscription *subscription = session.protocolVersion == 5
        ? client->subscribe(
              filter,
              QtMqttPropertyCodec::toQtSubscriptionProperties(entry.options),
              SessionConfig::sanitizeQos(entry.requestedQos))
        : client->subscribe(filter, SessionConfig::sanitizeQos(entry.requestedQos));
    if (!subscription) {
        entry.runtimeState = QStringLiteral("error");
        entry.lastError = tr("Qt MQTT returned no subscription object.");
        if (emitEvents) {
            m_eventHistoryService.appendEvent(
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

    if (emitEvents) {
        m_eventHistoryService.appendEvent(
            session,
            QStringLiteral("Subscription"),
            QStringLiteral("Requested %1 at QoS %2").arg(entry.topic).arg(entry.requestedQos));
    }
}

void SubscriptionService::observeSubscription(SessionState &session, SubscriptionEntry &entry, QMqttSubscription *subscription)
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
    connect(
        subscription,
        &QMqttSubscription::messageReceived,
        this,
        [this,
         sessionId = session.id,
         source = QPointer<QMqttSubscription>(subscription)](const QMqttMessage &message) {
            queueDeduplicatedIncomingMessage(sessionId, source, message);
        });
}

void SubscriptionService::queueDeduplicatedIncomingMessage(
    const QString &sessionId,
    const QPointer<QMqttSubscription> &source,
    const QMqttMessage &message)
{
    const auto previous = m_pendingIncomingDelivery.constFind(sessionId);
    if (previous != m_pendingIncomingDelivery.cend()
        && previous->message == message
        && previous->source != source) {
        return;
    }

    m_pendingIncomingDelivery.insert(sessionId, {message, source});
    m_eventHistoryService.queueIncomingMessage(
        sessionId,
        message.topic().name(),
        message.payload(),
        message.qos(),
        message.retain(),
        QtMqttPropertyCodec::fromQtPublishProperties(message.publishProperties()));

    // Overlapping subscription objects emit the same broker delivery synchronously.
    // Keep the identity only until control returns to the event loop.
    QTimer::singleShot(0, this, [this, sessionId, source, message]() {
        const auto current = m_pendingIncomingDelivery.constFind(sessionId);
        if (current != m_pendingIncomingDelivery.cend()
            && current->message == message
            && current->source == source) {
            m_pendingIncomingDelivery.erase(current);
        }
    });
}

void SubscriptionService::updateSubscriptionState(
    const QString &sessionId,
    const QString &topic,
    const QPointer<QMqttSubscription> &subscription,
    QMqttSubscription::SubscriptionState state)
{
    auto *session = m_sessionService.sessionById(sessionId);
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
            const MqttUserProperties responseProperties =
                QtMqttPropertyCodec::fromQtUserProperties(subscription->userProperties());
            const QString responseSuffix = responseProperties.isEmpty()
                ? QString()
                : QStringLiteral(" (%1)").arg(mqttUserPropertiesToText(responseProperties)
                                                   .replace(QLatin1Char('\n'), QStringLiteral(", ")));
            m_eventHistoryService.appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("Subscribed to %1%2").arg(entry->topic, responseSuffix));
        } else if (state == QMqttSubscription::Unsubscribed && entry->paused) {
            m_eventHistoryService.appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("Paused %1").arg(entry->topic));
        } else if (state == QMqttSubscription::Error) {
            const QString reason = entry->lastError.isEmpty()
                ? QStringLiteral("Broker returned a subscription error.")
                : entry->lastError;
            m_eventHistoryService.appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("%1 failed: %2").arg(entry->topic).arg(reason));
        }
    }

    emit subscriptionsChanged();
}

bool SubscriptionService::currentSessionHasActiveSubscriptionFps(qint64 nowMs) const
{
    const auto *session = m_sessionService.currentSession();
    if (!session) {
        return false;
    }

    for (const auto &subscription : session->subscriptions) {
        if (!subscription.paused
            && subscription.recentMessages.eventCount(nowMs) > 0) {
            return true;
        }
    }

    return false;
}
