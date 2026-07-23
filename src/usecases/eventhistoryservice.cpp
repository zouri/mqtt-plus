#include "eventhistoryservice.h"

#include "usecases/scriptservice.h"
#include "usecases/subscriptionservice.h"
#include "usecases/preferencescontroller.h"
#include "services/apputils.h"
#include "models/eventstreammodel.h"
#include "presentation/eventrenderer.h"
#include "services/payload/payloadcodec.h"
#include "services/storage/historystore.h"
#include "domain/messagerecord.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QHash>

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

struct MessageSubscriptionMatch {
    const SubscriptionEntry *displaySubscription = nullptr;
    QString topicColor;
    int payloadFormat = -1;
    bool refreshCurrentSubscriptionFps = false;
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
    return preview;
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

QHash<QString, QString> subscriptionAliases(const SessionState &session)
{
    QHash<QString, QString> aliases;
    for (const auto &subscription : session.subscriptions) {
        if (!subscription.alias.isEmpty()) {
            aliases.insert(subscription.topic, subscription.alias);
        }
    }
    return aliases;
}

QVariantMap messageRecordRow(const MessageRecord &record)
{
    QVariantMap row;
    row.insert(QStringLiteral("id"), record.id);
    row.insert(QStringLiteral("timestamp"), record.timestamp);
    row.insert(QStringLiteral("entry_type"), QStringLiteral("message"));
    row.insert(QStringLiteral("direction"), messageDirectionName(record.direction));
    row.insert(QStringLiteral("topic"), record.topic);
    row.insert(QStringLiteral("qos"), record.qos);
    row.insert(QStringLiteral("retain"), record.retain);
    row.insert(QStringLiteral("retain_known"), record.retainKnown);
    row.insert(QStringLiteral("payload_bytes"), record.payloadBytes);
    row.insert(QStringLiteral("payload_size"), record.payloadSize);
    row.insert(QStringLiteral("payload_state"), record.payloadState);
    row.insert(QStringLiteral("payload_preview"), record.payloadPreview);
    row.insert(QStringLiteral("payload_hash"), record.payloadHash);
    row.insert(QStringLiteral("payload_format"), record.payloadFormat);
    row.insert(QStringLiteral("parsed_payload"), record.parsedPayload);
    row.insert(QStringLiteral("parsed_format"), record.parsedFormat);
    row.insert(QStringLiteral("parse_error"), record.parseError);
    row.insert(QStringLiteral("script_id"), record.scriptId);
    row.insert(QStringLiteral("script_name"), record.scriptName);
    return row;
}

MessageSubscriptionMatch matchSubscriptionsForMessage(
    SessionState &session,
    const QString &topic,
    qint64 nowMs,
    bool isCurrentSession)
{
    MessageSubscriptionMatch match;
    int bestDisplayScore = -1;
    QString bestDisplayFilter;
    int bestColorScore = -1;
    QString bestColorFilter;

    for (auto &subscription : session.subscriptions) {
        if (!PayloadCodec::topicFilterMatches(subscription.topic, topic)) {
            continue;
        }

        if (!subscription.paused) {
            subscription.recentMessageTimestampsMs.append(nowMs);
            pruneRecentMessageTimestamps(subscription.recentMessageTimestampsMs, nowMs);
            match.refreshCurrentSubscriptionFps = match.refreshCurrentSubscriptionFps || isCurrentSession;
        }

        const int score = topicSpecificityScore(subscription.topic);
        if (score > bestDisplayScore
            || (score == bestDisplayScore && (bestDisplayFilter.isEmpty() || subscription.topic < bestDisplayFilter))) {
            bestDisplayScore = score;
            bestDisplayFilter = subscription.topic;
            match.displaySubscription = &subscription;
            match.payloadFormat = subscription.format;
        }

        if (!subscription.color.isEmpty()
            && (score > bestColorScore
                || (score == bestColorScore && (bestColorFilter.isEmpty() || subscription.topic < bestColorFilter)))) {
            bestColorScore = score;
            bestColorFilter = subscription.topic;
            match.topicColor = subscription.color;
        }
    }

    return match;
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
    session->runtime.totalMessageCount = 0;
    session->runtime.viewedMessageCount = 0;
    emit totalMessageCountChanged();
    m_messageStreamFrozen = false;
    m_frozenOldestLoadedMessageId = 0;
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
    const qint64 oldestLoadedMessageId = m_messageStreamFrozen
        ? m_frozenOldestLoadedMessageId
        : (session ? session->runtime.oldestLoadedMessageId : 0);
    if (!session || session->runtime.loadedAllMessageHistory || oldestLoadedMessageId <= 0) {
        return 0;
    }

    flushPendingMessageHistory();
    flushPendingVisibleMessageRows();

    const int pageSize = m_dependencies.preferencesController->historyPageSize();
    QVariantList rows = EventRenderer::loadHistoryRows(
        (*m_dependencies.historyStore).loadMessagesBefore(session->id, oldestLoadedMessageId, pageSize),
        session->runtime.subscriptionFormats,
        subscriptionColors(*session),
        subscriptionAliases(*session),
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

    if (m_messageStreamFrozen) {
        m_frozenOldestLoadedMessageId = EventRenderer::firstHistoryId(rows);
        (*m_dependencies.messagesModel).prependRows(rows);
        return rows.size();
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

bool EventHistoryService::messageStreamFrozen() const
{
    return m_messageStreamFrozen;
}

void EventHistoryService::setMessageStreamFrozen(bool frozen)
{
    if (m_messageStreamFrozen == frozen) {
        return;
    }

    if (frozen) {
        flushPendingVisibleMessageRows();
        if (m_dependencies.currentSessionState) {
            if (const auto *session = m_dependencies.currentSessionState()) {
                m_frozenOldestLoadedMessageId = session->runtime.oldestLoadedMessageId;
            }
        }
        m_messageStreamFrozen = true;
        return;
    }

    m_messageStreamFrozen = false;
    m_frozenOldestLoadedMessageId = 0;
    if (!m_dependencies.currentSessionState || !m_dependencies.messagesModel) {
        return;
    }

    auto *session = m_dependencies.currentSessionState();
    if (!session) {
        return;
    }

    if (session->runtime.totalMessageCount > session->runtime.messageRows.size()) {
        session->runtime.loadedAllMessageHistory = false;
    }

    if (m_pendingVisibleMessageSessionId == session->id) {
        m_pendingVisibleMessageRows.clear();
        m_pendingVisibleMessageSessionId.clear();
        m_visibleMessageRowsFlushTimer.stop();
    }
    (*m_dependencies.messagesModel).setRows(session->runtime.messageRows);
    m_dependencies.refreshScriptTestSamplesModel();
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

    if (!m_messageStreamFrozen) {
        if ((*m_dependencies.messagesModel).count() >= currentSession->runtime.messageRows.size()) {
            (*m_dependencies.messagesModel).setRows(currentSession->runtime.messageRows);
        } else if (rows.size() >= kMaxVisibleEventRows
                || (*m_dependencies.messagesModel).count() + rows.size() > kMaxVisibleEventRows * 2) {
            (*m_dependencies.messagesModel).setRows(currentSession->runtime.messageRows);
        } else {
            (*m_dependencies.messagesModel).appendRows(rows);
            (*m_dependencies.messagesModel).trimToLimit(kMaxVisibleEventRows);
        }
    }

    if (!rows.isEmpty()) {
        emit messageRowsAppended(rows.size());
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
    session->runtime.recentReceivedTimestampsMs.append(nowMs);
    pruneRecentMessageTimestamps(session->runtime.recentReceivedTimestampsMs, nowMs);
    const bool isCurrentSession = session == m_dependencies.currentSessionState();
    const MessageSubscriptionMatch subscriptionMatch = matchSubscriptionsForMessage(*session, topic, nowMs, isCurrentSession);

    if (session->outputPaused && !m_dependencies.preferencesController->saveMessagesWhenOutputPaused()) {
        if (subscriptionMatch.refreshCurrentSubscriptionFps && !(*m_dependencies.subscriptionFpsRefreshTimer).isActive()) {
            m_dependencies.refreshSubscriptionsModel();
            // subscriptionsChanged emitted by SubscriptionService
            (*m_dependencies.subscriptionFpsRefreshTimer).start();
        }
        return;
    }

    const SubscriptionEntry *displaySubscription = subscriptionMatch.displaySubscription;
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
    const bool processPayloadForVisibleOutput = !session->outputPaused;
    const bool hasScript = processPayloadForVisibleOutput && displaySubscription && !displaySubscription->scriptId.isEmpty();
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

    MessageRecord record;
    record.sessionId = sessionId;
    record.timestamp = timestamp;
    record.direction = MessageDirection::Incoming;
    record.topic = topic;
    record.payloadBytes = payloadPlan.storedBytes;
    record.parsedPayload = hasScript && scriptResult.success ? scriptResult.output : QString();
    record.parsedFormat = parsedFormat;
    record.parseError = parseError;
    record.scriptId = scriptId;
    record.scriptName = scriptDisplayName;
    record.payloadPreview = payloadPlan.preview;
    record.payloadState = payloadPlan.state;
    record.payloadSize = payloadPlan.originalSize;
    record.payloadHash = payloadPlan.hash;
    record.payloadFormat = subscriptionMatch.payloadFormat;
    const qint64 historyId = (*m_dependencies.historyStore).enqueueMessage(record);
    record.id = historyId;
    if (historyId <= 0) {
        reportMessageStorageError(
            *session,
            QStringLiteral("Cannot queue incoming message: %1").arg((*m_dependencies.historyStore).lastError()));
    } else {
        ++session->runtime.totalMessageCount;
        if (m_dependencies.currentSessionState && session == m_dependencies.currentSessionState()) {
            session->runtime.viewedMessageCount = session->runtime.totalMessageCount;
        }
        emit totalMessageCountChanged();
        m_lastMessageStorageError.clear();
        scheduleMessageHistoryFlush();
    }

    if (subscriptionMatch.refreshCurrentSubscriptionFps && !(*m_dependencies.subscriptionFpsRefreshTimer).isActive()) {
        m_dependencies.refreshSubscriptionsModel();
        // FPS timer started below if needed
        (*m_dependencies.subscriptionFpsRefreshTimer).start();
    }

    if (session != m_dependencies.currentSessionState() || session->outputPaused) {
        return;
    }

    QVariantMap historyRow = messageRecordRow(record);
    if (!subscriptionMatch.topicColor.isEmpty()) {
        historyRow.insert(QStringLiteral("topic_color"), subscriptionMatch.topicColor);
    }
    appendRenderedMessageRow(
        *session,
        EventRenderer::renderHistoryRow(
            historyRow,
            session->runtime.subscriptionFormats,
            subscriptionColors(*session),
            subscriptionAliases(*session)));
}

void EventHistoryService::appendPublishedMessage(
    const QString &sessionId,
    const QString &topic,
    const QByteArray &payloadBytes,
    int format,
    int qos,
    bool retain)
{
    auto *session = m_dependencies.sessionById(sessionId);
    if (!session) {
        return;
    }

    const QString timestamp = timestampNow();
    const PayloadStoragePlan payloadPlan = makePayloadStoragePlan(
        topic,
        payloadBytes,
        m_dependencies.preferencesController->maxIncomingPayloadBytes());
    if (payloadPlan.shouldReport) {
        appendEvent(*session, QStringLiteral("Payload"), payloadPlan.reportMessage);
    }

    MessageRecord record;
    record.sessionId = sessionId;
    record.timestamp = timestamp;
    record.direction = MessageDirection::Outgoing;
    record.topic = topic;
    record.qos = qos;
    record.retain = retain;
    record.retainKnown = true;
    record.payloadBytes = payloadPlan.storedBytes;
    record.payloadPreview = payloadPlan.preview;
    record.payloadState = payloadPlan.state;
    record.payloadSize = payloadPlan.originalSize;
    record.payloadHash = payloadPlan.hash;
    record.payloadFormat = format;
    const qint64 historyId = (*m_dependencies.historyStore).enqueueMessage(record);
    record.id = historyId;
    if (historyId <= 0) {
        reportMessageStorageError(
            *session,
            QStringLiteral("Cannot queue published message: %1").arg((*m_dependencies.historyStore).lastError()));
    } else {
        ++session->runtime.totalMessageCount;
        if (m_dependencies.currentSessionState && session == m_dependencies.currentSessionState()) {
            session->runtime.viewedMessageCount = session->runtime.totalMessageCount;
        }
        emit totalMessageCountChanged();
        m_lastMessageStorageError.clear();
        scheduleMessageHistoryFlush();
    }

    if (session != m_dependencies.currentSessionState()) {
        return;
    }

    const QVariantMap historyRow = messageRecordRow(record);

    QHash<QString, int> renderFormats = session->runtime.subscriptionFormats;
    renderFormats.insert(topic, format);
    appendRenderedMessageRow(
        *session,
        EventRenderer::renderHistoryRow(
            historyRow,
            renderFormats,
            subscriptionColors(*session),
            subscriptionAliases(*session)));
}

QString EventHistoryService::messagePayloadForReuse(
    qint64 messageId,
    const QString &fallbackPayload,
    const QString &fallbackTestPayload,
    int format) const
{
    const QString fallback = fallbackTestPayload.isEmpty() ? fallbackPayload : fallbackTestPayload;
    if (messageId <= 0 || !m_dependencies.historyStore) {
        return fallback;
    }

    const QByteArray payloadBytes = m_dependencies.historyStore->loadMessagePayloadBytes(messageId);
    if (payloadBytes.isEmpty()) {
        return fallback;
    }

    QString parseError;
    const QString decoded = PayloadCodec::decodeForDisplay(
        PayloadCodec::formatFromInt(format),
        payloadBytes,
        parseError);
    return parseError.isEmpty() ? decoded : fallback;
}

QString EventHistoryService::messagePayloadForDisplay(
    qint64 messageId,
    const QString &fallbackPayload,
    int format) const
{
    if (messageId <= 0 || !m_dependencies.historyStore) {
        return fallbackPayload;
    }

    const QByteArray payloadBytes = m_dependencies.historyStore->loadMessagePayloadBytes(messageId);
    if (payloadBytes.isEmpty()) {
        return fallbackPayload;
    }

    QString parseError;
    return PayloadCodec::decodeForDisplay(PayloadCodec::formatFromInt(format), payloadBytes, parseError);
}

QVariantMap EventHistoryService::messageDetails(qint64 messageId) const
{
    if (messageId <= 0 || !m_dependencies.historyStore) {
        return {};
    }

    const QVariantMap stored = m_dependencies.historyStore->loadMessage(messageId);
    if (stored.isEmpty()) {
        return {};
    }

    QHash<QString, int> formats;
    QHash<QString, QString> colors;
    QHash<QString, QString> aliases;
    if (const auto *session = m_dependencies.currentSessionState()) {
        formats = session->runtime.subscriptionFormats;
        colors = subscriptionColors(*session);
        aliases = subscriptionAliases(*session);
    }

    QVariantMap details = EventRenderer::renderHistoryRow(stored, formats, colors, aliases);
    const QByteArray payloadBytes = stored.value(QStringLiteral("payload_bytes")).toByteArray();
    const qint64 payloadSize = stored.value(QStringLiteral("payload_size")).toLongLong();
    const QString payloadState = stored.value(QStringLiteral("payload_state")).toString();
    const bool fullPayloadAvailable = payloadState != QStringLiteral("skipped")
        && (payloadSize == 0 || !payloadBytes.isEmpty());

    QString fullPayload;
    if (fullPayloadAvailable) {
        QString decodeError;
        fullPayload = PayloadCodec::decodeForDisplay(
            PayloadCodec::formatFromInt(stored.value(QStringLiteral("payload_format"), -1).toInt()),
            payloadBytes,
            decodeError);
        if (!decodeError.isEmpty()) {
            fullPayload = stored.value(QStringLiteral("payload_preview")).toString();
        }
    } else {
        fullPayload = stored.value(QStringLiteral("payload_preview")).toString();
    }

    details.insert(QStringLiteral("fullPayloadAvailable"), fullPayloadAvailable);
    details.insert(QStringLiteral("fullPayload"), fullPayload);
    details.insert(QStringLiteral("payloadPreview"), stored.value(QStringLiteral("payload_preview")));
    details.insert(QStringLiteral("payloadHash"), stored.value(QStringLiteral("payload_hash")));
    return details;
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
    m_messageStreamFrozen = false;
    m_frozenOldestLoadedMessageId = 0;

    const int pageSize = m_dependencies.preferencesController->historyPageSize();
    const QVariantList messageRows = (*m_dependencies.historyStore).loadMessages(session->id, pageSize);
    session->runtime.messageRows = EventRenderer::loadHistoryRows(
        messageRows,
        session->runtime.subscriptionFormats,
        subscriptionColors(*session),
        subscriptionAliases(*session),
        (*m_dependencies.launchTimestamp),
        true);
    session->runtime.totalMessageCount = (*m_dependencies.historyStore).totalMessageCount(session->id);
    session->runtime.viewedMessageCount = session->runtime.totalMessageCount;
    emit totalMessageCountChanged();
    session->runtime.oldestLoadedMessageId = EventRenderer::firstHistoryId(session->runtime.messageRows);
    session->runtime.loadedAllMessageHistory = messageRows.size() < pageSize;
    (*m_dependencies.messagesModel).setRows(session->runtime.messageRows);

    const QVariantList logRows = (*m_dependencies.historyStore).loadLogs(session->id, pageSize);
    session->runtime.logRows = EventRenderer::loadHistoryRows(
        logRows,
        session->runtime.subscriptionFormats,
        {},
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
