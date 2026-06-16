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
    session->messageRows.clear();
    session->oldestLoadedMessageId = 0;
    session->loadedAllMessageHistory = true;
    m_app.m_messagesModel.clear();
    m_app.refreshScriptTestSamplesModel();
    emit m_app.messageStreamChanged();
    emit m_app.scriptTestSamplesChanged();
}

void EventController::clearCurrentLogs()
{
    auto *session = m_app.currentSessionState();
    if (!session) {
        return;
    }

    m_app.m_historyStore.clearLogs(session->id);
    session->logRows.clear();
    session->oldestLoadedLogId = 0;
    session->loadedAllLogHistory = true;
    m_app.m_logsModel.clear();
    emit m_app.logStreamChanged();
}

int EventController::loadOlderCurrentSessionMessages()
{
    auto *session = m_app.currentSessionState();
    if (!session || session->loadedAllMessageHistory || session->oldestLoadedMessageId <= 0) {
        return 0;
    }

    const int pageSize = m_app.historyPageSize();
    QVariantList rows = EventRenderer::loadHistoryRows(
        m_app.m_historyStore.loadMessagesBefore(session->id, session->oldestLoadedMessageId, pageSize),
        session->subscriptionFormats,
        m_app.m_launchTimestamp,
        false);
    if (rows.isEmpty()) {
        session->loadedAllMessageHistory = true;
        return 0;
    }

    if (EventRenderer::containsRowsBeforeLaunch(rows, m_app.m_launchTimestamp)
            && EventRenderer::startsWithCurrentLaunchRows(session->messageRows, m_app.m_launchTimestamp)
            && !EventRenderer::containsLaunchDivider(session->messageRows)) {
        rows.append(EventRenderer::launchDividerRow(m_app.m_launchTimestamp));
    }

    QVariantList merged;
    merged.reserve(rows.size() + session->messageRows.size());
    for (const QVariant &item : rows) {
        merged.append(item);
    }
    for (const QVariant &item : session->messageRows) {
        merged.append(item);
    }
    session->messageRows = merged;
    session->oldestLoadedMessageId = EventRenderer::firstHistoryId(session->messageRows);
    m_app.m_messagesModel.prependRows(rows);
    m_app.refreshScriptTestSamplesModel();
    emit m_app.scriptTestSamplesChanged();
    return rows.size();
}

int EventController::loadOlderCurrentSessionLogs()
{
    auto *session = m_app.currentSessionState();
    if (!session || session->loadedAllLogHistory || session->oldestLoadedLogId <= 0) {
        return 0;
    }

    const int pageSize = m_app.historyPageSize();
    QVariantList rows = EventRenderer::loadHistoryRows(
        m_app.m_historyStore.loadLogsBefore(session->id, session->oldestLoadedLogId, pageSize),
        session->subscriptionFormats,
        m_app.m_launchTimestamp,
        false);
    if (rows.isEmpty()) {
        session->loadedAllLogHistory = true;
        return 0;
    }

    if (EventRenderer::containsRowsBeforeLaunch(rows, m_app.m_launchTimestamp)
            && EventRenderer::startsWithCurrentLaunchRows(session->logRows, m_app.m_launchTimestamp)
            && !EventRenderer::containsLaunchDivider(session->logRows)) {
        rows.append(EventRenderer::launchDividerRow(m_app.m_launchTimestamp));
    }

    QVariantList merged;
    merged.reserve(rows.size() + session->logRows.size());
    for (const QVariant &item : rows) {
        merged.append(item);
    }
    for (const QVariant &item : session->logRows) {
        merged.append(item);
    }
    session->logRows = merged;
    session->oldestLoadedLogId = EventRenderer::firstHistoryId(session->logRows);
    m_app.m_logsModel.prependRows(rows);
    return rows.size();
}

void EventController::appendRenderedMessageRow(SessionState &session, const QVariantMap &row)
{
    if (&session != m_app.currentSessionState()) {
        return;
    }

    session.messageRows.append(row);
    trimVisibleMessageRows(session);
    m_app.m_messagesModel.appendRow(row);
    m_app.m_messagesModel.trimToLimit(kMaxVisibleEventRows);
    emit m_app.messageStreamRowAppended(row);
    m_app.refreshScriptTestSamplesModel();
    emit m_app.scriptTestSamplesChanged();
}

void EventController::appendRenderedLogRow(SessionState &session, const QVariantMap &row)
{
    if (&session != m_app.currentSessionState()) {
        return;
    }

    session.logRows.append(row);
    trimVisibleLogRows(session);
    m_app.m_logsModel.appendRow(row);
    m_app.m_logsModel.trimToLimit(kMaxVisibleEventRows);
    emit m_app.logStreamRowAppended(row);
}

void EventController::appendEvent(SessionState &session, const QString &channel, const QString &message)
{
    const QString timestamp = timestampNow();
    const qint64 historyId = m_app.m_historyStore.appendEvent(session.id, timestamp, channel, message);
    m_app.m_historyStore.pruneLogs(session.id, m_app.logRetentionLimit());

    appendRenderedLogRow(session, EventRenderer::eventRow(historyId, timestamp, channel, message));
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
        subscription.receivedMessageCount += 1;
        subscription.lastMessageTimestamp = timestamp;
        refreshCurrentSubscriptionFps = refreshCurrentSubscriptionFps || session == m_app.currentSessionState();
    }

    if (session->outputPaused && !m_app.saveMessagesWhenOutputPaused()) {
        if (refreshCurrentSubscriptionFps && !m_app.m_subscriptionFpsRefreshTimer.isActive()) {
            m_app.refreshSubscriptionsModel();
            emit m_app.subscriptionsChanged();
            m_app.m_subscriptionFpsRefreshTimer.start();
        }
        return;
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
    if (historyId <= 0 && !m_app.m_historyStore.lastError().isEmpty()) {
        const QString storageError =
            QStringLiteral("Cannot save incoming message: %1").arg(m_app.m_historyStore.lastError());
        if (storageError != m_lastMessageStorageError) {
            m_lastMessageStorageError = storageError;
            appendEvent(*session, QStringLiteral("Storage"), storageError);
        }
    } else {
        m_lastMessageStorageError.clear();
    }
    m_app.m_historyStore.pruneMessages(sessionId, m_app.messageRetentionLimit());

    if (refreshCurrentSubscriptionFps && !m_app.m_subscriptionFpsRefreshTimer.isActive()) {
        m_app.refreshSubscriptionsModel();
        emit m_app.subscriptionsChanged();
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
    appendRenderedMessageRow(*session, EventRenderer::renderHistoryRow(historyRow, session->subscriptionFormats));
}

void EventController::trimVisibleMessageRows(SessionState &session)
{
    const qsizetype overflow = session.messageRows.size() - kMaxVisibleEventRows;
    if (overflow <= 0) {
        return;
    }

    session.messageRows.remove(0, overflow);
    session.oldestLoadedMessageId = EventRenderer::firstHistoryId(session.messageRows);
    session.loadedAllMessageHistory = false;
}

void EventController::trimVisibleLogRows(SessionState &session)
{
    const qsizetype overflow = session.logRows.size() - kMaxVisibleEventRows;
    if (overflow <= 0) {
        return;
    }

    session.logRows.remove(0, overflow);
    session.oldestLoadedLogId = EventRenderer::firstHistoryId(session.logRows);
    session.loadedAllLogHistory = false;
}

void EventController::reloadCurrentSessionHistory()
{
    auto *session = m_app.currentSessionState();
    if (!session) {
        return;
    }
    const int pageSize = m_app.historyPageSize();
    const QVariantList messageRows = m_app.m_historyStore.loadMessages(session->id, pageSize);
    session->messageRows = EventRenderer::loadHistoryRows(
        messageRows,
        session->subscriptionFormats,
        m_app.m_launchTimestamp,
        true);
    session->oldestLoadedMessageId = EventRenderer::firstHistoryId(session->messageRows);
    session->loadedAllMessageHistory = messageRows.size() < pageSize;
    m_app.m_messagesModel.setRows(session->messageRows);

    const QVariantList logRows = m_app.m_historyStore.loadLogs(session->id, pageSize);
    session->logRows = EventRenderer::loadHistoryRows(
        logRows,
        session->subscriptionFormats,
        m_app.m_launchTimestamp,
        true);
    session->oldestLoadedLogId = EventRenderer::firstHistoryId(session->logRows);
    session->loadedAllLogHistory = logRows.size() < pageSize;
    m_app.m_logsModel.setRows(session->logRows);

    m_app.refreshScriptTestSamplesModel();
    emit m_app.scriptTestSamplesChanged();
}
