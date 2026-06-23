#include "eventcontroller.h"

#include "app/appfacadeutils.h"
#include "domain/script.h"
#include "presentation/eventrenderer.h"
#include "services/payload/payloadcodec.h"

#include <QDateTime>

#include <utility>
using namespace AppFacadeUtils;

namespace {
constexpr int kVisibleMessageRowsFlushIntervalMs = 16;
constexpr int kMessageHistoryFlushIntervalMs = 250;
constexpr int kMessageHistoryFlushBatchSize = 200;
}

EventController::EventController(QObject *parent)
    : QObject(parent)
{
    m_messageHistoryFlushTimer.setInterval(kMessageHistoryFlushIntervalMs);
    m_messageHistoryFlushTimer.setSingleShot(true);
    m_visibleMessageRowsFlushTimer.setInterval(kVisibleMessageRowsFlushIntervalMs);
    m_visibleMessageRowsFlushTimer.setSingleShot(true);
    connect(
        &m_messageHistoryFlushTimer,
        &QTimer::timeout,
        this,
        &EventController::flushPendingMessageHistory);
    connect(
        &m_visibleMessageRowsFlushTimer,
        &QTimer::timeout,
        this,
        &EventController::flushPendingVisibleMessageRows);
}

void EventController::setDependencies(Dependencies dependencies)
{
    m_dependencies = std::move(dependencies);
}

void EventController::clearCurrentMessages()
{
    auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session || !m_dependencies.historyStore || !m_dependencies.messagesModel) {
        return;
    }

    m_dependencies.historyStore->clearMessages(session->id);
    session->messageRows.clear();
    session->oldestLoadedMessageId = 0;
    session->loadedAllMessageHistory = true;
    if (m_pendingVisibleMessageSessionId == session->id) {
        m_pendingVisibleMessageRows.clear();
        m_pendingVisibleMessageSessionId.clear();
    }
    m_dependencies.messagesModel->clear();
    if (m_dependencies.refreshScriptTestSamplesModel) {
        m_dependencies.refreshScriptTestSamplesModel();
    }
    if (m_dependencies.emitMessageStreamChanged) {
        m_dependencies.emitMessageStreamChanged();
    }
    if (m_dependencies.emitScriptTestSamplesChanged) {
        m_dependencies.emitScriptTestSamplesChanged();
    }
}

void EventController::clearCurrentLogs()
{
    auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session || !m_dependencies.historyStore || !m_dependencies.logsModel) {
        return;
    }

    m_dependencies.historyStore->clearLogs(session->id);
    session->logRows.clear();
    session->oldestLoadedLogId = 0;
    session->loadedAllLogHistory = true;
    m_dependencies.logsModel->clear();
    if (m_dependencies.emitLogStreamChanged) {
        m_dependencies.emitLogStreamChanged();
    }
}

int EventController::loadOlderCurrentSessionMessages()
{
    auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session || !m_dependencies.historyStore || !m_dependencies.messagesModel
            || session->loadedAllMessageHistory || session->oldestLoadedMessageId <= 0) {
        return 0;
    }

    flushPendingMessageHistory();
    flushPendingVisibleMessageRows();

    const int pageSize = m_dependencies.historyPageSize ? m_dependencies.historyPageSize() : 0;
    QVariantList rows = EventRenderer::loadHistoryRows(
        m_dependencies.historyStore->loadMessagesBefore(session->id, session->oldestLoadedMessageId, pageSize),
        session->subscriptionFormats,
        m_dependencies.launchTimestamp,
        false);
    if (rows.isEmpty()) {
        session->loadedAllMessageHistory = true;
        return 0;
    }

    if (EventRenderer::containsRowsBeforeLaunch(rows, m_dependencies.launchTimestamp)
            && EventRenderer::startsWithCurrentLaunchRows(session->messageRows, m_dependencies.launchTimestamp)
            && !EventRenderer::containsLaunchDivider(session->messageRows)) {
        rows.append(EventRenderer::launchDividerRow(m_dependencies.launchTimestamp));
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
    m_dependencies.messagesModel->prependRows(rows);
    if (m_dependencies.refreshScriptTestSamplesModel) {
        m_dependencies.refreshScriptTestSamplesModel();
    }
    if (m_dependencies.emitScriptTestSamplesChanged) {
        m_dependencies.emitScriptTestSamplesChanged();
    }
    return rows.size();
}

int EventController::loadOlderCurrentSessionLogs()
{
    auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session || !m_dependencies.historyStore || !m_dependencies.logsModel
            || session->loadedAllLogHistory || session->oldestLoadedLogId <= 0) {
        return 0;
    }

    const int pageSize = m_dependencies.historyPageSize ? m_dependencies.historyPageSize() : 0;
    QVariantList rows = EventRenderer::loadHistoryRows(
        m_dependencies.historyStore->loadLogsBefore(session->id, session->oldestLoadedLogId, pageSize),
        session->subscriptionFormats,
        m_dependencies.launchTimestamp,
        false);
    if (rows.isEmpty()) {
        session->loadedAllLogHistory = true;
        return 0;
    }

    if (EventRenderer::containsRowsBeforeLaunch(rows, m_dependencies.launchTimestamp)
            && EventRenderer::startsWithCurrentLaunchRows(session->logRows, m_dependencies.launchTimestamp)
            && !EventRenderer::containsLaunchDivider(session->logRows)) {
        rows.append(EventRenderer::launchDividerRow(m_dependencies.launchTimestamp));
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
    m_dependencies.logsModel->prependRows(rows);
    return rows.size();
}

void EventController::appendRenderedMessageRow(SessionState &session, const QVariantMap &row)
{
    auto *currentSession = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (&session != currentSession || !m_dependencies.messagesModel) {
        return;
    }

    session.messageRows.append(row);
    trimVisibleMessageRows(session);

    if (!m_pendingVisibleMessageSessionId.isEmpty()
        && m_pendingVisibleMessageSessionId != session.id) {
        flushPendingVisibleMessageRows();
    }
    m_pendingVisibleMessageSessionId = session.id;
    m_pendingVisibleMessageRows.append(row);
    scheduleVisibleMessageRowsFlush();
}

void EventController::flushPendingVisibleMessageRows()
{
    if (m_pendingVisibleMessageRows.isEmpty() || !m_dependencies.messagesModel) {
        return;
    }

    auto *currentSession = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    const bool stillShowingSession = currentSession
        && currentSession->id == m_pendingVisibleMessageSessionId;
    const QVariantMap lastRow = m_pendingVisibleMessageRows.constLast().toMap();
    const QVariantList rows = std::exchange(m_pendingVisibleMessageRows, {});
    m_pendingVisibleMessageSessionId.clear();

    if (!stillShowingSession) {
        return;
    }

    if (rows.size() >= kMaxVisibleEventRows
        || m_dependencies.messagesModel->count() + rows.size() > kMaxVisibleEventRows * 2) {
        m_dependencies.messagesModel->setRows(currentSession->messageRows);
    } else {
        m_dependencies.messagesModel->appendRows(rows);
        m_dependencies.messagesModel->trimToLimit(kMaxVisibleEventRows);
    }

    if (m_dependencies.emitMessageStreamRowAppended) {
        m_dependencies.emitMessageStreamRowAppended(lastRow);
    }
    if (m_dependencies.refreshScriptTestSamplesModel) {
        m_dependencies.refreshScriptTestSamplesModel();
    }
    if (m_dependencies.emitScriptTestSamplesChanged) {
        m_dependencies.emitScriptTestSamplesChanged();
    }
}

void EventController::appendRenderedLogRow(SessionState &session, const QVariantMap &row)
{
    auto *currentSession = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (&session != currentSession || !m_dependencies.logsModel) {
        return;
    }

    session.logRows.append(row);
    trimVisibleLogRows(session);
    m_dependencies.logsModel->appendRow(row);
    m_dependencies.logsModel->trimToLimit(kMaxVisibleEventRows);
    if (m_dependencies.emitLogStreamRowAppended) {
        m_dependencies.emitLogStreamRowAppended(row);
    }
}

void EventController::appendEvent(SessionState &session, const QString &channel, const QString &message)
{
    if (!m_dependencies.historyStore) {
        return;
    }

    const QString timestamp = timestampNow();
    const qint64 historyId = m_dependencies.historyStore->appendEvent(session.id, timestamp, channel, message);
    const int logRetentionLimit = m_dependencies.logRetentionLimit ? m_dependencies.logRetentionLimit() : 0;
    m_dependencies.historyStore->pruneLogs(session.id, logRetentionLimit);

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

    const auto *script = m_dependencies.scriptById ? m_dependencies.scriptById(subscription->scriptId) : nullptr;
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
    auto *session = m_dependencies.sessionById ? m_dependencies.sessionById(sessionId) : nullptr;
    if (!session || !m_dependencies.historyStore) {
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
        refreshCurrentSubscriptionFps = refreshCurrentSubscriptionFps
            || (m_dependencies.currentSession && session == m_dependencies.currentSession());
    }

    const bool saveWhenPaused = m_dependencies.saveMessagesWhenOutputPaused
        ? m_dependencies.saveMessagesWhenOutputPaused()
        : false;
    if (session->outputPaused && !saveWhenPaused) {
        const bool fpsRefreshActive = m_dependencies.subscriptionFpsRefreshActive
            && m_dependencies.subscriptionFpsRefreshActive();
        if (refreshCurrentSubscriptionFps && !fpsRefreshActive) {
            if (m_dependencies.refreshSubscriptionsModel) {
                m_dependencies.refreshSubscriptionsModel();
            }
            if (m_dependencies.emitSubscriptionsChanged) {
                m_dependencies.emitSubscriptionsChanged();
            }
            if (m_dependencies.startSubscriptionFpsRefresh) {
                m_dependencies.startSubscriptionFpsRefresh();
            }
        }
        return;
    }

    const SubscriptionEntry *displaySubscription = m_dependencies.bestSubscriptionForTopic
        ? m_dependencies.bestSubscriptionForTopic(*session, topic)
        : nullptr;
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

    const qint64 historyId = m_dependencies.historyStore->enqueueMessage(
        sessionId,
        timestamp,
        topic,
        payloadBytes,
        hasScript && scriptResult.success ? scriptResult.output : QString(),
        parsedFormat,
        parseError,
        scriptId,
        scriptDisplayName);
    if (historyId <= 0) {
        reportMessageStorageError(
            *session,
            QStringLiteral("Cannot queue incoming message: %1").arg(m_dependencies.historyStore->lastError()));
    } else {
        m_lastMessageStorageError.clear();
        scheduleMessageHistoryFlush();
    }

    const bool fpsRefreshActive = m_dependencies.subscriptionFpsRefreshActive
        && m_dependencies.subscriptionFpsRefreshActive();
    if (refreshCurrentSubscriptionFps && !fpsRefreshActive) {
        if (m_dependencies.refreshSubscriptionsModel) {
            m_dependencies.refreshSubscriptionsModel();
        }
        if (m_dependencies.emitSubscriptionsChanged) {
            m_dependencies.emitSubscriptionsChanged();
        }
        if (m_dependencies.startSubscriptionFpsRefresh) {
            m_dependencies.startSubscriptionFpsRefresh();
        }
    }

    const auto *currentSession = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (session != currentSession || session->outputPaused) {
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
    auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session || !m_dependencies.historyStore || !m_dependencies.messagesModel || !m_dependencies.logsModel) {
        return;
    }
    flushPendingMessageHistory();
    flushPendingVisibleMessageRows();

    const int pageSize = m_dependencies.historyPageSize ? m_dependencies.historyPageSize() : 0;
    const QVariantList messageRows = m_dependencies.historyStore->loadMessages(session->id, pageSize);
    session->messageRows = EventRenderer::loadHistoryRows(
        messageRows,
        session->subscriptionFormats,
        m_dependencies.launchTimestamp,
        true);
    session->oldestLoadedMessageId = EventRenderer::firstHistoryId(session->messageRows);
    session->loadedAllMessageHistory = messageRows.size() < pageSize;
    m_dependencies.messagesModel->setRows(session->messageRows);

    const QVariantList logRows = m_dependencies.historyStore->loadLogs(session->id, pageSize);
    session->logRows = EventRenderer::loadHistoryRows(
        logRows,
        session->subscriptionFormats,
        m_dependencies.launchTimestamp,
        true);
    session->oldestLoadedLogId = EventRenderer::firstHistoryId(session->logRows);
    session->loadedAllLogHistory = logRows.size() < pageSize;
    m_dependencies.logsModel->setRows(session->logRows);

    if (m_dependencies.refreshScriptTestSamplesModel) {
        m_dependencies.refreshScriptTestSamplesModel();
    }
    if (m_dependencies.emitScriptTestSamplesChanged) {
        m_dependencies.emitScriptTestSamplesChanged();
    }
}

void EventController::flushPendingMessageHistory()
{
    if (!m_dependencies.historyStore || m_dependencies.historyStore->pendingMessageCount() <= 0) {
        return;
    }

    const QStringList flushedSessionIds = m_dependencies.historyStore->flushPendingMessages();
    if (flushedSessionIds.isEmpty() && !m_dependencies.historyStore->lastError().isEmpty()) {
        if (auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr) {
            reportMessageStorageError(
                *session,
                QStringLiteral("Cannot save queued messages: %1").arg(m_dependencies.historyStore->lastError()));
        }
        return;
    }

    const int messageRetentionLimit = m_dependencies.messageRetentionLimit
        ? m_dependencies.messageRetentionLimit()
        : 0;
    if (messageRetentionLimit > 0) {
        for (const QString &flushedSessionId : flushedSessionIds) {
            m_dependencies.historyStore->pruneMessages(flushedSessionId, messageRetentionLimit);
        }
    }
    m_lastMessageStorageError.clear();
}

void EventController::reportMessageStorageError(SessionState &session, const QString &message)
{
    if (message == m_lastMessageStorageError) {
        return;
    }

    m_lastMessageStorageError = message;
    appendEvent(session, QStringLiteral("Storage"), message);
}

void EventController::scheduleMessageHistoryFlush()
{
    if (!m_dependencies.historyStore) {
        return;
    }

    if (m_dependencies.historyStore->pendingMessageCount() >= kMessageHistoryFlushBatchSize) {
        flushPendingMessageHistory();
        return;
    }

    if (!m_messageHistoryFlushTimer.isActive()) {
        m_messageHistoryFlushTimer.start();
    }
}

void EventController::scheduleVisibleMessageRowsFlush()
{
    if (!m_visibleMessageRowsFlushTimer.isActive()) {
        m_visibleMessageRowsFlushTimer.start();
    }
}
