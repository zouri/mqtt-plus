#include "eventcontroller.h"

#include "app/appfacade.h"
#include "app/appfacadeutils.h"
#include "presentation/eventrenderer.h"
#include "services/payload/payloadcodec.h"

#include <QDateTime>

using namespace AppFacadeUtils;

EventController::EventController(AppFacade *app, QObject *parent)
    : QObject(parent)
    , m_app(*app)
{
}

void EventController::clearCurrentMessages()
{
    auto *session = m_app.currentSessionState();
    if (!session) {
        return;
    }

    m_app.m_historyStore.clearMessages(session->id);
    session->eventRows.clear();
    session->oldestLoadedEventId = 0;
    session->loadedAllEventHistory = true;
    m_app.m_eventsModel.clear();
    m_app.refreshScriptTestSamplesModel();
    emit m_app.eventStreamChanged();
    emit m_app.scriptTestSamplesChanged();
}

int EventController::loadOlderCurrentSessionEvents()
{
    auto *session = m_app.currentSessionState();
    if (!session || session->loadedAllEventHistory || session->oldestLoadedEventId <= 0) {
        return 0;
    }

    QVariantList rows = EventRenderer::loadHistoryRows(
        m_app.m_historyStore.loadEntriesBefore(session->id, session->oldestLoadedEventId, kEventPageSize),
        session->subscriptionFormats,
        m_app.m_launchTimestamp,
        false);
    if (rows.isEmpty()) {
        session->loadedAllEventHistory = true;
        return 0;
    }

    if (EventRenderer::containsRowsBeforeLaunch(rows, m_app.m_launchTimestamp)
            && EventRenderer::startsWithCurrentLaunchRows(session->eventRows, m_app.m_launchTimestamp)
            && !EventRenderer::containsLaunchDivider(session->eventRows)) {
        rows.append(EventRenderer::launchDividerRow(m_app.m_launchTimestamp));
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
    m_app.m_eventsModel.prependRows(rows);
    m_app.refreshScriptTestSamplesModel();
    emit m_app.scriptTestSamplesChanged();
    return rows.size();
}

void EventController::appendRenderedEventRow(SessionState &session, const QVariantMap &row)
{
    if (&session != m_app.currentSessionState()) {
        return;
    }

    session.eventRows.append(row);
    trimVisibleEventRows(session);
    m_app.m_eventsModel.appendRow(row);
    m_app.m_eventsModel.trimToLimit(kMaxVisibleEventRows);
    emit m_app.eventStreamRowAppended(row);
    if (row.value(QStringLiteral("kind")).toString() == QStringLiteral("message")) {
        m_app.refreshScriptTestSamplesModel();
        emit m_app.scriptTestSamplesChanged();
    }
}

void EventController::appendEvent(SessionState &session, const QString &channel, const QString &message)
{
    const QString timestamp = timestampNow();
    const qint64 historyId = m_app.m_historyStore.appendEvent(session.id, timestamp, channel, message);

    appendRenderedEventRow(session, EventRenderer::eventRow(historyId, timestamp, channel, message));
}

LuaScriptResult EventController::parseIncomingPayload(
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

    const auto *script = m_app.m_scriptController.scriptById(subscription->scriptId);
    if (!script) {
        LuaScriptResult result;
        result.error = tr("Selected Lua script is missing.");
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

void EventController::appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes)
{
    auto *session = m_app.sessionById(sessionId);
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
        refreshCurrentSubscriptionFps = refreshCurrentSubscriptionFps || session == m_app.currentSessionState();
    }

    const SubscriptionEntry *displaySubscription = m_app.m_subscriptionController.bestSubscriptionForTopic(*session, topic);
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

    const qint64 historyId = m_app.m_historyStore.appendMessage(
        sessionId,
        timestamp,
        topic,
        payloadBytes,
        hasScript && scriptResult.success ? scriptResult.output : QString(),
        parsedFormat,
        parseError,
        scriptId,
        scriptDisplayName);

    if (refreshCurrentSubscriptionFps && !m_app.m_subscriptionFpsRefreshTimer.isActive()) {
        m_app.m_subscriptionFpsRefreshTimer.start();
    }

    if (session != m_app.currentSessionState() || session->outputPaused) {
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

void EventController::trimVisibleEventRows(SessionState &session)
{
    const qsizetype overflow = session.eventRows.size() - kMaxVisibleEventRows;
    if (overflow <= 0) {
        return;
    }

    session.eventRows.remove(0, overflow);
    session.oldestLoadedEventId = EventRenderer::firstHistoryId(session.eventRows);
    session.loadedAllEventHistory = false;
}

void EventController::reloadCurrentSessionHistory()
{
    auto *session = m_app.currentSessionState();
    if (!session) {
        return;
    }
    const QVariantList rows = m_app.m_historyStore.loadEntries(session->id, kEventPageSize);
    session->eventRows = EventRenderer::loadHistoryRows(
        rows,
        session->subscriptionFormats,
        m_app.m_launchTimestamp,
        true);
    session->oldestLoadedEventId = EventRenderer::firstHistoryId(session->eventRows);
    session->loadedAllEventHistory = rows.size() < kEventPageSize;
    m_app.m_eventsModel.setRows(session->eventRows);
    m_app.refreshScriptTestSamplesModel();
    emit m_app.scriptTestSamplesChanged();
}
