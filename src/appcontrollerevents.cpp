#include "appcontroller.h"

#include "appcontrollerutils.h"
#include "eventrenderer.h"

#include <QDateTime>

using namespace AppControllerUtils;

void AppController::clearCurrentMessages()
{
    auto *session = currentSessionState();
    if (!session) {
        return;
    }

    m_historyStore.clearMessages(session->id);
    session->eventRows.clear();
    session->oldestLoadedEventId = 0;
    session->loadedAllEventHistory = true;
    emit eventStreamChanged();
    emit scriptTestSamplesChanged();
}

QVariantList AppController::loadOlderCurrentEventRows()
{
    auto *session = currentSessionState();
    if (!session || session->loadedAllEventHistory || session->oldestLoadedEventId <= 0) {
        return {};
    }

    QVariantList rows = EventRenderer::loadHistoryRows(
        m_historyStore.loadEntriesBefore(session->id, session->oldestLoadedEventId, kEventPageSize),
        session->subscriptionFormats,
        m_launchTimestamp,
        false);
    if (rows.isEmpty()) {
        session->loadedAllEventHistory = true;
        return {};
    }

    if (EventRenderer::containsRowsBeforeLaunch(rows, m_launchTimestamp)
            && EventRenderer::startsWithCurrentLaunchRows(session->eventRows, m_launchTimestamp)
            && !EventRenderer::containsLaunchDivider(session->eventRows)) {
        rows.append(EventRenderer::launchDividerRow(m_launchTimestamp));
    }

    QVariantList merged;
    merged.reserve(rows.size() + session->eventRows.size());
    for (const QVariant &item : rows) {
        merged.append(item);
    }
    for (const QVariant &item : session->eventRows) {
        merged.append(item);
    }
    session->eventRows = merged;
    session->oldestLoadedEventId = EventRenderer::firstHistoryId(session->eventRows);
    emit scriptTestSamplesChanged();
    return rows;
}

void AppController::appendRenderedEventRow(SessionState &session, const QVariantMap &row)
{
    if (&session != currentSessionState()) {
        return;
    }

    session.eventRows.append(row);
    trimVisibleEventRows(session);
    emit eventStreamRowAppended(row);
    if (row.value(QStringLiteral("kind")).toString() == QStringLiteral("message")) {
        emit scriptTestSamplesChanged();
    }
}

void AppController::appendEvent(SessionState &session, const QString &channel, const QString &message)
{
    const QString timestamp = timestampNow();
    const qint64 historyId = m_historyStore.appendEvent(session.id, timestamp, channel, message);

    appendRenderedEventRow(session, EventRenderer::eventRow(historyId, timestamp, channel, message));
}

LuaScriptResult AppController::parseIncomingPayload(
    const SessionState &session,
    const SubscriptionEntry *subscription,
    const QString &topic,
    const QByteArray &payloadBytes,
    const QString &timestamp,
    QString &scriptNameOut,
    QString &decodedPayloadOut) const
{
    const PayloadFormat format = subscription
        ? PayloadCodec::formatFromInt(subscription->format)
        : PayloadCodec::resolveTopicFormat(session.subscriptionFormats, topic);

    QString decodeError;
    decodedPayloadOut = PayloadCodec::decodeForDisplay(format, payloadBytes, decodeError);

    if (!subscription || subscription->scriptId.isEmpty()) {
        return {};
    }

    const auto *script = scriptById(subscription->scriptId);
    if (!script) {
        LuaScriptResult result;
        result.error = QStringLiteral("Selected Lua script is missing.");
        return result;
    }

    scriptNameOut = script->name;

    LuaScriptContext context;
    context.topic = topic;
    context.payloadBytes = payloadBytes;
    context.decodedPayload = decodedPayloadOut;
    context.decodeError = decodeError;
    context.format = format;
    context.timestamp = timestamp;
    return LuaRunner::run(script->code, context);
}

void AppController::appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes)
{
    auto *session = sessionById(sessionId);
    if (!session) {
        return;
    }

    const QString timestamp = timestampNow();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    bool refreshCurrentSubscriptionFps = false;

    for (auto &subscription : session->subscriptions) {
        if (!PayloadCodec::topicFilterMatches(subscription.topic, topic)) {
            continue;
        }

        subscription.recentMessageTimestampsMs.append(nowMs);
        pruneRecentMessageTimestamps(subscription.recentMessageTimestampsMs, nowMs);
        refreshCurrentSubscriptionFps = refreshCurrentSubscriptionFps || session == currentSessionState();
    }

    const SubscriptionEntry *displaySubscription = bestSubscriptionForTopic(*session, topic);
    QString scriptDisplayName;
    QString decodedPayload;
    const LuaScriptResult scriptResult = parseIncomingPayload(
        *session,
        displaySubscription,
        topic,
        payloadBytes,
        timestamp,
        scriptDisplayName,
        decodedPayload);
    const bool hasScript = displaySubscription && !displaySubscription->scriptId.isEmpty();
    const QString scriptId = hasScript ? displaySubscription->scriptId : QString();
    const QString parsedFormat = hasScript && scriptResult.success
        ? QStringLiteral("Lua: %1").arg(scriptDisplayName)
        : QString();
    const QString parseError = hasScript && !scriptResult.success
        ? scriptResult.error
        : QString();

    const qint64 historyId = m_historyStore.appendMessage(
        sessionId,
        timestamp,
        topic,
        payloadBytes,
        hasScript && scriptResult.success ? scriptResult.output : QString(),
        parsedFormat,
        parseError,
        scriptId,
        scriptDisplayName);

    if (refreshCurrentSubscriptionFps && !m_subscriptionFpsRefreshTimer.isActive()) {
        m_subscriptionFpsRefreshTimer.start();
    }

    if (session != currentSessionState() || session->outputPaused) {
        return;
    }

    QVariantMap historyRow;
    historyRow.insert(QStringLiteral("id"), historyId);
    historyRow.insert(QStringLiteral("timestamp"), timestamp);
    historyRow.insert(QStringLiteral("entry_type"), QStringLiteral("message"));
    historyRow.insert(QStringLiteral("topic"), topic);
    historyRow.insert(QStringLiteral("payload"), QString::fromUtf8(payloadBytes));
    historyRow.insert(QStringLiteral("payload_b64"), QString::fromLatin1(payloadBytes.toBase64()));
    historyRow.insert(QStringLiteral("parsed_payload"), hasScript && scriptResult.success ? scriptResult.output : QString());
    historyRow.insert(QStringLiteral("parsed_format"), parsedFormat);
    historyRow.insert(QStringLiteral("parse_error"), parseError);
    historyRow.insert(QStringLiteral("script_id"), scriptId);
    historyRow.insert(QStringLiteral("script_name"), scriptDisplayName);
    appendRenderedEventRow(*session, EventRenderer::renderHistoryRow(historyRow, session->subscriptionFormats));
}

void AppController::trimVisibleEventRows(SessionState &session)
{
    const qsizetype overflow = session.eventRows.size() - kMaxVisibleEventRows;
    if (overflow <= 0) {
        return;
    }

    session.eventRows.remove(0, overflow);
    session.oldestLoadedEventId = EventRenderer::firstHistoryId(session.eventRows);
    session.loadedAllEventHistory = false;
}

void AppController::reloadCurrentSessionHistory()
{
    auto *session = currentSessionState();
    if (!session) {
        return;
    }
    const QVariantList rows = m_historyStore.loadEntries(session->id, kEventPageSize);
    session->eventRows = EventRenderer::loadHistoryRows(
        rows,
        session->subscriptionFormats,
        m_launchTimestamp,
        true);
    session->oldestLoadedEventId = EventRenderer::firstHistoryId(session->eventRows);
    session->loadedAllEventHistory = rows.size() < kEventPageSize;
    emit scriptTestSamplesChanged();
}

