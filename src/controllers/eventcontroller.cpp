#include "eventcontroller.h"

#include "app/appfacade.h"
#include "app/appfacadeutils.h"
#include "presentation/eventrenderer.h"
#include "services/payload/payloadcodec.h"

#include <QCryptographicHash>
#include <QDateTime>

#include <algorithm>

using namespace AppFacadeUtils;

namespace {
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
}

EventController::EventController(AppFacade *app, QObject *parent)
    : QObject(parent)
    , m_app(*app)
{
    m_messageHistoryFlushTimer.setInterval(kMessageHistoryFlushIntervalMs);
    m_messageHistoryFlushTimer.setSingleShot(true);
    connect(
        &m_messageHistoryFlushTimer,
        &QTimer::timeout,
        this,
        &EventController::flushPendingMessageHistory);
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

    flushPendingMessageHistory();

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
    const PayloadStoragePlan payloadPlan = makePayloadStoragePlan(
        topic,
        payloadBytes,
        m_app.maxIncomingPayloadBytes());
    if (payloadPlan.shouldReport) {
        appendEvent(*session, QStringLiteral("Payload"), payloadPlan.reportMessage);
    }

    QString scriptDisplayName;
    QString decodedPayload;
    const bool hasScript = displaySubscription && !displaySubscription->scriptId.isEmpty();
    LuaScriptResult scriptResult;
    if (payloadPlan.allowFullProcessing) {
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
        scriptDisplayName = m_app.scriptName(displaySubscription->scriptId);
    }
    const QString scriptId = hasScript ? displaySubscription->scriptId : QString();
    const QString parsedFormat = hasScript && scriptResult.success
        ? QStringLiteral("Lua: %1").arg(scriptDisplayName)
        : QString();
    const QString parseError = hasScript && !scriptResult.success
        ? scriptResult.error
        : QString();

    const qint64 historyId = m_app.m_historyStore.enqueueMessage(
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
            QStringLiteral("Cannot queue incoming message: %1").arg(m_app.m_historyStore.lastError()));
    } else {
        m_lastMessageStorageError.clear();
        scheduleMessageHistoryFlush();
    }

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
    historyRow.insert(QStringLiteral("payload"), payloadPlan.preview);
    historyRow.insert(QStringLiteral("payload_b64"), QString());
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
    flushPendingMessageHistory();

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

void EventController::flushPendingMessageHistory()
{
    if (m_app.m_historyStore.pendingMessageCount() <= 0) {
        return;
    }

    const QStringList flushedSessionIds = m_app.m_historyStore.flushPendingMessages();
    if (flushedSessionIds.isEmpty() && !m_app.m_historyStore.lastError().isEmpty()) {
        if (auto *session = m_app.currentSessionState()) {
            reportMessageStorageError(
                *session,
                QStringLiteral("Cannot save queued messages: %1").arg(m_app.m_historyStore.lastError()));
        }
        return;
    }

    if (m_app.messageRetentionLimit() > 0) {
        for (const QString &flushedSessionId : flushedSessionIds) {
            m_app.m_historyStore.pruneMessages(flushedSessionId, m_app.messageRetentionLimit());
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
    if (m_app.m_historyStore.pendingMessageCount() >= kMessageHistoryFlushBatchSize) {
        flushPendingMessageHistory();
        return;
    }

    if (!m_messageHistoryFlushTimer.isActive()) {
        m_messageHistoryFlushTimer.start();
    }
}
