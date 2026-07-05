#include "eventhistoryservice.h"

#include "usecases/scriptservice.h"
#include "usecases/subscriptionservice.h"
#include "usecases/preferencescontroller.h"
#include "services/apputils.h"
#include "models/eventstreammodel.h"
#include "presentation/eventrenderer.h"
#include "services/payload/payloadcodec.h"
#include "services/storage/historystore.h"

#include <QCryptographicHash>
#include <QDateTime>

#include <algorithm>

using namespace AppUtils;

namespace {
constexpr int kVisibleMessageRowsFlushIntervalMs = 16;
constexpr int kMessageHistoryFlushIntervalMs = 250;
constexpr int kMessageHistoryFlushBatchSize = 200;
constexpr qint64 kPayloadPreviewBytes = 64 * 1024;
constexpr qint64 kHardPayloadLimitBytes = 16 * 1024 * 1024;

struct PayloadStoragePlan {
    QByteArray storedBytes;
    QString preview;
    QString state = QStringLiteral("full");
    QString hash;
    qint64 originalSize = 0;
    bool allowFullProcessing = true;
    bool shouldReport = false;
    QString reportMessage;
};

QString formatByteCount(qint64 bytes)
{
    if (bytes >= 1024 * 1024) {
        return QStringLiteral("%1 MiB").arg(QString::number(bytes / 1024.0 / 1024.0, 'f', 1));
    }
    if (bytes >= 1024) {
        return QStringLiteral("%1 KiB").arg(QString::number(bytes / 1024.0, 'f', 1));
    }
    return QStringLiteral("%1 bytes").arg(bytes);
}

bool looksBinary(const QByteArray &bytes)
{
    if (bytes.isEmpty()) {
        return false;
    }

    const qsizetype sampleSize = (std::min)(bytes.size(), qsizetype(4096));
    qsizetype suspicious = 0;
    for (qsizetype i = 0; i < sampleSize; ++i) {
        const uchar ch = static_cast<uchar>(bytes.at(i));
        if (ch == 0 || (ch < 0x20 && ch != '\n' && ch != '\r' && ch != '\t')) {
            ++suspicious;
        }
    }
    return suspicious > 0 || suspicious * 100 > sampleSize * 15;
}

QString payloadTextPreview(const QByteArray &bytes)
{
    const QByteArray previewBytes = bytes.left((std::min)(bytes.size(), qsizetype(kPayloadPreviewBytes)));
    return QString::fromUtf8(previewBytes);
}

QString payloadHexPreview(const QByteArray &bytes)
{
    const QByteArray previewBytes = bytes.left((std::min)(bytes.size(), qsizetype(64)));
    QString preview = QString::fromLatin1(previewBytes.toHex(' ').toUpper());
    if (bytes.size() > previewBytes.size()) {
        preview.append(QStringLiteral(" ..."));
    }
    return QStringLiteral("Raw preview (hex): %1").arg(preview);
}

PayloadStoragePlan makePayloadStoragePlan(const QString &topic, const QByteArray &payloadBytes, int configuredLimit)
{
    PayloadStoragePlan plan;
    plan.originalSize = payloadBytes.size();

    const qint64 maxBytes = configuredLimit > 0 ? configuredLimit : kHardPayloadLimitBytes;
    const bool binary = looksBinary(payloadBytes);
    plan.preview = binary ? payloadHexPreview(payloadBytes) : payloadTextPreview(payloadBytes);

    if (plan.originalSize > maxBytes) {
        plan.state = QStringLiteral("skipped");
        plan.allowFullProcessing = false;
        plan.hash = QString::fromLatin1(
            QCryptographicHash::hash(payloadBytes, QCryptographicHash::Sha256).toHex());
        plan.shouldReport = true;
        plan.reportMessage = QStringLiteral(
            "Payload skipped on %1: %2 exceeds the configured limit of %3. SHA-256: %4")
            .arg(topic, formatByteCount(plan.originalSize), formatByteCount(maxBytes), plan.hash);
        return plan;
    }

    plan.storedBytes = payloadBytes;
    if (binary) {
        plan.state = QStringLiteral("raw_only");
        plan.shouldReport = true;
        plan.reportMessage = QStringLiteral("Payload stored as raw bytes on %1: %2.")
            .arg(topic, formatByteCount(plan.originalSize));
    } else if (plan.originalSize > kPayloadPreviewBytes) {
        plan.state = QStringLiteral("truncated");
        plan.shouldReport = true;
        plan.reportMessage = QStringLiteral("Payload truncated for display on %1: showing %2 of %3.")
            .arg(topic, formatByteCount(kPayloadPreviewBytes), formatByteCount(plan.originalSize));
    }

    return plan;
}

QHash<QString, QString> subscriptionColors(const SessionState &session)
{
    QHash<QString, QString> colors;
    for (const auto &subscription : session.subscriptions) {
        if (!subscription.color.isEmpty()) {
            colors.insert(subscription.topic, subscription.color);
        }
    }
    return colors;
}
}

EventHistoryService::EventHistoryService(QObject *parent)
    : QObject(parent)
{
}

void EventHistoryService::setDependencies(const Dependencies &dependencies)
{
    m_dependencies = dependencies;

    m_messageHistoryFlushTimer.setInterval(kMessageHistoryFlushIntervalMs);
    m_messageHistoryFlushTimer.setSingleShot(true);
    m_visibleMessageRowsFlushTimer.setInterval(kVisibleMessageRowsFlushIntervalMs);
    m_visibleMessageRowsFlushTimer.setSingleShot(true);
    connect(
        &m_messageHistoryFlushTimer,
        &QTimer::timeout,
        this,
        &EventHistoryService::flushPendingMessageHistory,
        Qt::UniqueConnection);
    connect(
        &m_visibleMessageRowsFlushTimer,
        &QTimer::timeout,
        this,
        &EventHistoryService::flushPendingVisibleMessageRows,
        Qt::UniqueConnection);
}

void EventHistoryService::clearCurrentMessages()
{
    auto *session = m_dependencies.currentSessionState();
    if (!session) {
        return;
    }

    (*m_dependencies.historyStore).clearMessages(session->id);
    session->runtime.messageRows.clear();
    session->runtime.oldestLoadedMessageId = 0;
    session->runtime.loadedAllMessageHistory = true;
    if (m_pendingVisibleMessageSessionId == session->id) {
        m_pendingVisibleMessageRows.clear();
        m_pendingVisibleMessageSessionId.clear();
    }
    (*m_dependencies.messagesModel).clear();
    m_dependencies.refreshScriptTestSamplesModel();
    emit messageStreamChanged();
}

void EventHistoryService::clearCurrentLogs()
{
    auto *session = m_dependencies.currentSessionState();
    if (!session) {
        return;
    }

    (*m_dependencies.historyStore).clearLogs(session->id);
    session->runtime.logRows.clear();
    session->runtime.oldestLoadedLogId = 0;
    session->runtime.loadedAllLogHistory = true;
    (*m_dependencies.logsModel).clear();
    emit logStreamChanged();
}

int EventHistoryService::loadOlderCurrentSessionMessages()
{
    auto *session = m_dependencies.currentSessionState();
    if (!session || session->runtime.loadedAllMessageHistory || session->runtime.oldestLoadedMessageId <= 0) {
        return 0;
    }

    flushPendingMessageHistory();
    flushPendingVisibleMessageRows();

    const int pageSize = m_dependencies.preferencesController->historyPageSize();
    QVariantList rows = EventRenderer::loadHistoryRows(
        (*m_dependencies.historyStore).loadMessagesBefore(session->id, session->runtime.oldestLoadedMessageId, pageSize),
        session->runtime.subscriptionFormats,
        subscriptionColors(*session),
        (*m_dependencies.launchTimestamp),
        false);
    if (rows.isEmpty()) {
        session->runtime.loadedAllMessageHistory = true;
        return 0;
    }

    if (EventRenderer::containsRowsBeforeLaunch(rows, (*m_dependencies.launchTimestamp))
            && EventRenderer::startsWithCurrentLaunchRows(session->runtime.messageRows, (*m_dependencies.launchTimestamp))
            && !EventRenderer::containsLaunchDivider(session->runtime.messageRows)) {
        rows.append(EventRenderer::launchDividerRow((*m_dependencies.launchTimestamp)));
    }

    QVariantList merged;
    merged.reserve(rows.size() + session->runtime.messageRows.size());
    for (const QVariant &item : rows) {
        merged.append(item);
    }
    for (const QVariant &item : session->runtime.messageRows) {
        merged.append(item);
    }
    session->runtime.messageRows = merged;
    session->runtime.oldestLoadedMessageId = EventRenderer::firstHistoryId(session->runtime.messageRows);
    (*m_dependencies.messagesModel).prependRows(rows);
    m_dependencies.refreshScriptTestSamplesModel();
    return rows.size();
}

int EventHistoryService::loadOlderCurrentSessionLogs()
{
    auto *session = m_dependencies.currentSessionState();
    if (!session || session->runtime.loadedAllLogHistory || session->runtime.oldestLoadedLogId <= 0) {
        return 0;
    }

    const int pageSize = m_dependencies.preferencesController->historyPageSize();
    QVariantList rows = EventRenderer::loadHistoryRows(
        (*m_dependencies.historyStore).loadLogsBefore(session->id, session->runtime.oldestLoadedLogId, pageSize),
        session->runtime.subscriptionFormats,
        {},
        (*m_dependencies.launchTimestamp),
        false);
    if (rows.isEmpty()) {
        session->runtime.loadedAllLogHistory = true;
        return 0;
    }

    if (EventRenderer::containsRowsBeforeLaunch(rows, (*m_dependencies.launchTimestamp))
            && EventRenderer::startsWithCurrentLaunchRows(session->runtime.logRows, (*m_dependencies.launchTimestamp))
            && !EventRenderer::containsLaunchDivider(session->runtime.logRows)) {
        rows.append(EventRenderer::launchDividerRow((*m_dependencies.launchTimestamp)));
    }

    QVariantList merged;
    merged.reserve(rows.size() + session->runtime.logRows.size());
    for (const QVariant &item : rows) {
        merged.append(item);
    }
    for (const QVariant &item : session->runtime.logRows) {
        merged.append(item);
    }
    session->runtime.logRows = merged;
    session->runtime.oldestLoadedLogId = EventRenderer::firstHistoryId(session->runtime.logRows);
    (*m_dependencies.logsModel).prependRows(rows);
    return rows.size();
}

void EventHistoryService::appendRenderedMessageRow(SessionState &session, const QVariantMap &row)
{
    if (&session != m_dependencies.currentSessionState()) {
        return;
    }

    session.runtime.messageRows.append(row);
    trimVisibleMessageRows(session);

    if (!m_pendingVisibleMessageSessionId.isEmpty()
            && m_pendingVisibleMessageSessionId != session.id) {
        flushPendingVisibleMessageRows();
    }
    m_pendingVisibleMessageSessionId = session.id;
    m_pendingVisibleMessageRows.append(row);
    scheduleVisibleMessageRowsFlush();
}

void EventHistoryService::flushPendingVisibleMessageRows()
{
    if (m_pendingVisibleMessageRows.isEmpty()) {
        return;
    }

    auto *currentSession = m_dependencies.currentSessionState();
    const bool stillShowingSession = currentSession
        && currentSession->id == m_pendingVisibleMessageSessionId;
    const QVariantList rows = m_pendingVisibleMessageRows;
    m_pendingVisibleMessageRows.clear();
    m_pendingVisibleMessageSessionId.clear();

    if (!stillShowingSession) {
        return;
    }

    if (rows.size() >= kMaxVisibleEventRows
            || (*m_dependencies.messagesModel).count() + rows.size() > kMaxVisibleEventRows * 2) {
        (*m_dependencies.messagesModel).setRows(currentSession->runtime.messageRows);
    } else {
        (*m_dependencies.messagesModel).appendRows(rows);
        (*m_dependencies.messagesModel).trimToLimit(kMaxVisibleEventRows);
    }

    for (const QVariant &row : rows) {
        emit messageAppended(row.toMap());
    }
    m_dependencies.refreshScriptTestSamplesModel();
}

void EventHistoryService::appendRenderedLogRow(SessionState &session, const QVariantMap &row)
{
    if (&session != m_dependencies.currentSessionState()) {
        return;
    }

    session.runtime.logRows.append(row);
    trimVisibleLogRows(session);
    (*m_dependencies.logsModel).appendRow(row);
    (*m_dependencies.logsModel).trimToLimit(kMaxVisibleEventRows);
    emit logAppended(row);
}

void EventHistoryService::appendEvent(SessionState &session, const QString &channel, const QString &message)
{
    const QString timestamp = timestampNow();
    const qint64 historyId = (*m_dependencies.historyStore).appendEvent(session.id, timestamp, channel, message);
    (*m_dependencies.historyStore).pruneLogs(session.id, m_dependencies.preferencesController->logRetentionLimit());

    appendRenderedLogRow(session, EventRenderer::eventRow(historyId, timestamp, channel, message));
}

LuaScriptResult EventHistoryService::parseIncomingPayload(
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
        : PayloadCodec::resolveTopicFormat(session.runtime.subscriptionFormats, topic);

    QString decodeError;
    decodedPayloadOut = PayloadCodec::decodeForDisplay(format, payloadBytes, decodeError);

    if (!subscription || subscription->scriptId.isEmpty()) {
        return {};
    }

    const auto *script = (*m_dependencies.scriptController).scriptById(subscription->scriptId);
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

void EventHistoryService::appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes)
{
    auto *session = m_dependencies.sessionById(sessionId);
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
        refreshCurrentSubscriptionFps = refreshCurrentSubscriptionFps || session == m_dependencies.currentSessionState();
    }

    if (session->outputPaused && !m_dependencies.preferencesController->saveMessagesWhenOutputPaused()) {
        if (refreshCurrentSubscriptionFps && !(*m_dependencies.subscriptionFpsRefreshTimer).isActive()) {
            m_dependencies.refreshSubscriptionsModel();
            // subscriptionsChanged emitted by SubscriptionService
            (*m_dependencies.subscriptionFpsRefreshTimer).start();
        }
        return;
    }

    const SubscriptionEntry *displaySubscription = (*m_dependencies.subscriptionController).bestSubscriptionForTopic(*session, topic);
    const PayloadStoragePlan payloadPlan = makePayloadStoragePlan(
        topic,
        payloadBytes,
        m_dependencies.preferencesController->maxIncomingPayloadBytes());
    if (payloadPlan.shouldReport) {
        const QString reportKey = QStringLiteral("%1|%2|%3").arg(session->id, topic, payloadPlan.state);
        if (!m_reportedPayloadStorageStates.contains(reportKey)) {
            m_reportedPayloadStorageStates.insert(reportKey);
            appendEvent(*session, QStringLiteral("Payload"), payloadPlan.reportMessage);
        }
    }

    QString scriptDisplayName;
    QString decodedPayload;
    const bool hasScript = displaySubscription && !displaySubscription->scriptId.isEmpty();
    LuaScriptResult scriptResult;
    if (hasScript && payloadPlan.allowFullProcessing) {
        scriptResult = parseIncomingPayload(
            *session,
            displaySubscription,
            topic,
            payloadBytes,
            timestamp,
            scriptDisplayName,
            decodedPayload);
    } else if (hasScript) {
        scriptResult.error = QStringLiteral("Lua script skipped because payload exceeds the configured size limit.");
        scriptDisplayName = m_dependencies.scriptController->scriptName(displaySubscription->scriptId);
    }
    const QString scriptId = hasScript ? displaySubscription->scriptId : QString();
    const QString parsedFormat = hasScript && scriptResult.success
        ? QStringLiteral("Lua: %1").arg(scriptDisplayName)
        : QString();
    const QString parseError = hasScript && !scriptResult.success
        ? scriptResult.error
        : QString();

    const qint64 historyId = (*m_dependencies.historyStore).enqueueMessage(
        sessionId,
        timestamp,
        topic,
        payloadPlan.storedBytes,
        hasScript && scriptResult.success ? scriptResult.output : QString(),
        parsedFormat,
        parseError,
        scriptId,
        scriptDisplayName,
        payloadPlan.preview,
        payloadPlan.state,
        payloadPlan.originalSize,
        payloadPlan.hash);
    if (historyId <= 0) {
        reportMessageStorageError(
            *session,
            QStringLiteral("Cannot queue incoming message: %1").arg((*m_dependencies.historyStore).lastError()));
    } else {
        m_lastMessageStorageError.clear();
        scheduleMessageHistoryFlush();
    }

    if (refreshCurrentSubscriptionFps && !(*m_dependencies.subscriptionFpsRefreshTimer).isActive()) {
        m_dependencies.refreshSubscriptionsModel();
        // FPS timer started below if needed
        (*m_dependencies.subscriptionFpsRefreshTimer).start();
    }

    if (session != m_dependencies.currentSessionState() || session->outputPaused) {
        return;
    }

    QVariantMap historyRow;
    historyRow.insert(QStringLiteral("id"), historyId);
    historyRow.insert(QStringLiteral("timestamp"), timestamp);
    historyRow.insert(QStringLiteral("entry_type"), QStringLiteral("message"));
    historyRow.insert(QStringLiteral("topic"), topic);
    historyRow.insert(QStringLiteral("payload"), payloadPlan.preview);
    historyRow.insert(QStringLiteral("payload_b64"), QStringLiteral(""));
    historyRow.insert(QStringLiteral("payload_bytes"), payloadPlan.storedBytes);
    historyRow.insert(QStringLiteral("payload_size"), payloadPlan.originalSize);
    historyRow.insert(QStringLiteral("payload_state"), payloadPlan.state);
    historyRow.insert(QStringLiteral("payload_preview"), payloadPlan.preview);
    historyRow.insert(QStringLiteral("payload_hash"), payloadPlan.hash);
    historyRow.insert(QStringLiteral("parsed_payload"), hasScript && scriptResult.success ? scriptResult.output : QString());
    historyRow.insert(QStringLiteral("parsed_format"), parsedFormat);
    historyRow.insert(QStringLiteral("parse_error"), parseError);
    historyRow.insert(QStringLiteral("script_id"), scriptId);
    historyRow.insert(QStringLiteral("script_name"), scriptDisplayName);
    if (displaySubscription && !displaySubscription->color.isEmpty()) {
        historyRow.insert(QStringLiteral("topic_color"), displaySubscription->color);
    }
    appendRenderedMessageRow(
        *session,
        EventRenderer::renderHistoryRow(historyRow, session->runtime.subscriptionFormats, subscriptionColors(*session)));
}

void EventHistoryService::trimVisibleMessageRows(SessionState &session)
{
    const qsizetype overflow = session.runtime.messageRows.size() - kMaxVisibleEventRows;
    if (overflow <= 0) {
        return;
    }

    session.runtime.messageRows.remove(0, overflow);
    session.runtime.oldestLoadedMessageId = EventRenderer::firstHistoryId(session.runtime.messageRows);
    session.runtime.loadedAllMessageHistory = false;
}

void EventHistoryService::trimVisibleLogRows(SessionState &session)
{
    const qsizetype overflow = session.runtime.logRows.size() - kMaxVisibleEventRows;
    if (overflow <= 0) {
        return;
    }

    session.runtime.logRows.remove(0, overflow);
    session.runtime.oldestLoadedLogId = EventRenderer::firstHistoryId(session.runtime.logRows);
    session.runtime.loadedAllLogHistory = false;
}

void EventHistoryService::reloadCurrentSessionHistory()
{
    auto *session = m_dependencies.currentSessionState();
    if (!session) {
        return;
    }
    flushPendingVisibleMessageRows();
    flushPendingMessageHistory();

    const int pageSize = m_dependencies.preferencesController->historyPageSize();
    const QVariantList messageRows = (*m_dependencies.historyStore).loadMessages(session->id, pageSize);
    session->runtime.messageRows = EventRenderer::loadHistoryRows(
        messageRows,
        session->runtime.subscriptionFormats,
        subscriptionColors(*session),
        (*m_dependencies.launchTimestamp),
        true);
    session->runtime.oldestLoadedMessageId = EventRenderer::firstHistoryId(session->runtime.messageRows);
    session->runtime.loadedAllMessageHistory = messageRows.size() < pageSize;
    (*m_dependencies.messagesModel).setRows(session->runtime.messageRows);

    const QVariantList logRows = (*m_dependencies.historyStore).loadLogs(session->id, pageSize);
    session->runtime.logRows = EventRenderer::loadHistoryRows(
        logRows,
        session->runtime.subscriptionFormats,
        {},
        (*m_dependencies.launchTimestamp),
        true);
    session->runtime.oldestLoadedLogId = EventRenderer::firstHistoryId(session->runtime.logRows);
    session->runtime.loadedAllLogHistory = logRows.size() < pageSize;
    (*m_dependencies.logsModel).setRows(session->runtime.logRows);

    m_dependencies.refreshScriptTestSamplesModel();
}

void EventHistoryService::flushPendingMessageHistory()
{
    if ((*m_dependencies.historyStore).pendingMessageCount() <= 0) {
        return;
    }

    const QStringList flushedSessionIds = (*m_dependencies.historyStore).flushPendingMessages();
    if (flushedSessionIds.isEmpty() && !(*m_dependencies.historyStore).lastError().isEmpty()) {
        if (auto *session = m_dependencies.currentSessionState()) {
            reportMessageStorageError(
                *session,
                QStringLiteral("Cannot save queued messages: %1").arg((*m_dependencies.historyStore).lastError()));
        }
        return;
    }

    if (m_dependencies.preferencesController->messageRetentionLimit() > 0) {
        for (const QString &flushedSessionId : flushedSessionIds) {
            (*m_dependencies.historyStore).pruneMessages(flushedSessionId, m_dependencies.preferencesController->messageRetentionLimit());
        }
    }
    m_lastMessageStorageError.clear();
}

void EventHistoryService::reportMessageStorageError(SessionState &session, const QString &message)
{
    if (message == m_lastMessageStorageError) {
        return;
    }

    m_lastMessageStorageError = message;
    appendEvent(session, QStringLiteral("Storage"), message);
}

void EventHistoryService::scheduleMessageHistoryFlush()
{
    if ((*m_dependencies.historyStore).pendingMessageCount() >= kMessageHistoryFlushBatchSize) {
        flushPendingMessageHistory();
        return;
    }

    if (!m_messageHistoryFlushTimer.isActive()) {
        m_messageHistoryFlushTimer.start();
    }
}

void EventHistoryService::scheduleVisibleMessageRowsFlush()
{
    if (!m_visibleMessageRowsFlushTimer.isActive()) {
        m_visibleMessageRowsFlushTimer.start();
    }
}
