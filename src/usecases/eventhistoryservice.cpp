#include "eventhistoryservice.h"

#include "usecases/sessionservice.h"
#include "usecases/scriptservice.h"
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
    bool currentSubscriptionActivity = false;
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
            match.currentSubscriptionActivity = match.currentSubscriptionActivity || isCurrentSession;
        }

        const int score = PayloadCodec::topicSpecificityScore(subscription.topic);
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

QVariantList &EventHistoryService::streamRows(SessionState &session, Stream kind)
{
    return kind == Stream::Message ? session.runtime.messageRows : session.runtime.logRows;
}

qint64 &EventHistoryService::oldestLoadedId(SessionState &session, Stream kind)
{
    return kind == Stream::Message ? session.runtime.oldestLoadedMessageId : session.runtime.oldestLoadedLogId;
}

bool &EventHistoryService::loadedAllHistory(SessionState &session, Stream kind)
{
    return kind == Stream::Message ? session.runtime.loadedAllMessageHistory : session.runtime.loadedAllLogHistory;
}

EventHistoryService::EventHistoryService(
    SessionService &sessionService,
    HistoryStore &historyStore,
    EventStreamModel &messages,
    EventStreamModel &logs,
    ScriptService &scriptService,
    QString launchTimestamp,
    PreferencesController &preferencesController,
    QObject *parent)
    : QObject(parent)
    , m_sessionService(sessionService)
    , m_historyStore(historyStore)
    , m_messages(messages)
    , m_logs(logs)
    , m_scriptService(scriptService)
    , m_launchTimestamp(std::move(launchTimestamp))
    , m_preferencesController(preferencesController)
{
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
    connect(
        &m_scriptService,
        &ScriptService::scriptsChanged,
        this,
        [this]() { m_luaRuntimeCache.clear(); });
}

bool EventHistoryService::clearStream(Stream kind, bool allSessions)
{
    auto *current = m_sessionService.currentSession();
    const bool isMessage = kind == Stream::Message;

    bool ok;
    if (allSessions) {
        ok = isMessage ? m_historyStore.clearAllMessages() : m_historyStore.clearAllLogs();
    } else {
        if (!current) {
            return false;
        }
        ok = isMessage ? m_historyStore.clearMessages(current->id) : m_historyStore.clearLogs(current->id);
    }
    if (!ok) {
        if (current) {
            const QString message = isMessage
                ? tr("Cannot clear message history: %1").arg(m_historyStore.lastError())
                : tr("Cannot clear log history: %1").arg(m_historyStore.lastError());
            reportMessageStorageError(*current, message);
        }
        return false;
    }

    auto clearRuntime = [kind](SessionState &session) {
        streamRows(session, kind).clear();
        if (kind == Stream::Message) {
            session.runtime.totalMessageCount = 0;
            session.runtime.viewedMessageCount = 0;
        }
        oldestLoadedId(session, kind) = 0;
        loadedAllHistory(session, kind) = true;
    };
    if (allSessions) {
        for (SessionState &session : m_sessionService.sessions()) {
            clearRuntime(session);
        }
    } else {
        clearRuntime(*current);
    }

    if (isMessage) {
        resetMessageStreamTransientState(allSessions, current);
    }
    (isMessage ? m_messages : m_logs).clear();
    m_lastMessageStorageError.clear();
    if (isMessage) {
        emit totalMessageCountChanged();
        emit messageStreamChanged();
    } else {
        emit logStreamChanged();
    }
    return true;
}

void EventHistoryService::resetMessageStreamTransientState(bool allSessions, const SessionState *current)
{
    m_messageHistoryFlushTimer.stop();
    m_messageStreamFrozen = false;
    m_frozenOldestLoadedMessageId = 0;
    if (allSessions || (current && m_pendingVisibleMessageSessionId == current->id)) {
        m_pendingVisibleMessageRows.clear();
        m_pendingVisibleMessageSessionId.clear();
        m_visibleMessageRowsFlushTimer.stop();
    }
}

bool EventHistoryService::clearCurrentMessages()
{
    return clearStream(Stream::Message, false);
}

bool EventHistoryService::clearCurrentLogs()
{
    return clearStream(Stream::Log, false);
}

bool EventHistoryService::clearAllMessages()
{
    return clearStream(Stream::Message, true);
}

bool EventHistoryService::clearAllLogs()
{
    return clearStream(Stream::Log, true);
}

bool EventHistoryService::clearAllHistory()
{
    auto *current = m_sessionService.currentSession();
    if (!m_historyStore.clearAllHistory()) {
        if (current) {
            reportMessageStorageError(
                *current,
                tr("Cannot clear history: %1").arg(m_historyStore.lastError()));
        }
        return false;
    }

    for (SessionState &session : m_sessionService.sessions()) {
        streamRows(session, Stream::Message).clear();
        streamRows(session, Stream::Log).clear();
        session.runtime.totalMessageCount = 0;
        session.runtime.viewedMessageCount = 0;
        oldestLoadedId(session, Stream::Message) = 0;
        oldestLoadedId(session, Stream::Log) = 0;
        loadedAllHistory(session, Stream::Message) = true;
        loadedAllHistory(session, Stream::Log) = true;
    }
    resetMessageStreamTransientState(true, nullptr);
    m_messages.clear();
    m_logs.clear();
    m_lastMessageStorageError.clear();
    emit totalMessageCountChanged();
    emit messageStreamChanged();
    emit logStreamChanged();
    return true;
}

int EventHistoryService::loadOlderCurrentSession(Stream kind)
{
    auto *session = m_sessionService.currentSession();
    const bool isMessage = kind == Stream::Message;
    const bool frozen = isMessage && m_messageStreamFrozen;
    const qint64 oldestId = frozen
        ? m_frozenOldestLoadedMessageId
        : (session ? oldestLoadedId(*session, kind) : 0);
    if (!session || loadedAllHistory(*session, kind) || oldestId <= 0) {
        return 0;
    }

    if (isMessage) {
        flushPendingMessageHistory();
        flushPendingVisibleMessageRows();
    }

    const int pageSize = m_preferencesController.historyPageSize();
    QVariantList rows = EventRenderer::loadHistoryRows(
        isMessage
            ? m_historyStore.loadMessagesBefore(session->id, oldestId, pageSize)
            : m_historyStore.loadLogsBefore(session->id, oldestId, pageSize),
        session->runtime.subscriptionFormats,
        isMessage ? subscriptionColors(*session) : QHash<QString, QString> {},
        isMessage ? subscriptionAliases(*session) : QHash<QString, QString> {},
        m_launchTimestamp,
        false);
    if (rows.isEmpty()) {
        loadedAllHistory(*session, kind) = true;
        return 0;
    }

    auto &currentRows = streamRows(*session, kind);
    if (EventRenderer::containsRowsBeforeLaunch(rows, m_launchTimestamp)
            && EventRenderer::startsWithCurrentLaunchRows(currentRows, m_launchTimestamp)
            && !EventRenderer::containsLaunchDivider(currentRows)) {
        rows.append(EventRenderer::launchDividerRow(m_launchTimestamp));
    }

    auto &model = isMessage ? m_messages : m_logs;
    if (frozen) {
        m_frozenOldestLoadedMessageId = EventRenderer::firstHistoryId(rows);
        model.prependRows(rows);
        return rows.size();
    }

    QVariantList merged = rows;
    merged.append(currentRows);
    currentRows = merged;
    oldestLoadedId(*session, kind) = EventRenderer::firstHistoryId(currentRows);
    model.prependRows(rows);
    return rows.size();
}

int EventHistoryService::loadOlderCurrentSessionMessages()
{
    return loadOlderCurrentSession(Stream::Message);
}

int EventHistoryService::loadOlderCurrentSessionLogs()
{
    return loadOlderCurrentSession(Stream::Log);
}

void EventHistoryService::setMessageStreamFrozen(bool frozen)
{
    if (m_messageStreamFrozen == frozen) {
        return;
    }

    if (frozen) {
        flushPendingVisibleMessageRows();
        if (const auto *session = m_sessionService.currentSession()) {
            m_frozenOldestLoadedMessageId = session->runtime.oldestLoadedMessageId;
        }
        m_messageStreamFrozen = true;
        return;
    }

    m_messageStreamFrozen = false;
    m_frozenOldestLoadedMessageId = 0;
    auto *session = m_sessionService.currentSession();
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
    m_messages.setRows(session->runtime.messageRows);
}

void EventHistoryService::appendRenderedMessageRow(SessionState &session, const QVariantMap &row)
{
    if (&session != m_sessionService.currentSession()) {
        return;
    }

    session.runtime.messageRows.append(row);
    trimVisibleRows(session, Stream::Message);

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

    auto *currentSession = m_sessionService.currentSession();
    const bool stillShowingSession = currentSession
        && currentSession->id == m_pendingVisibleMessageSessionId;
    const QVariantList rows = m_pendingVisibleMessageRows;
    m_pendingVisibleMessageRows.clear();
    m_pendingVisibleMessageSessionId.clear();

    if (!stillShowingSession) {
        return;
    }

    if (!m_messageStreamFrozen) {
        if (m_messages.rowCount() >= currentSession->runtime.messageRows.size()) {
            m_messages.setRows(currentSession->runtime.messageRows);
        } else if (rows.size() >= kMaxVisibleEventRows
                || m_messages.rowCount() + rows.size() > kMaxVisibleEventRows * 2) {
            m_messages.setRows(currentSession->runtime.messageRows);
        } else {
            m_messages.appendRows(rows);
            m_messages.trimToLimit(kMaxVisibleEventRows);
        }
    }

    if (!rows.isEmpty()) {
        emit messageRowsAppended(rows.size());
    }
}

void EventHistoryService::appendRenderedLogRow(SessionState &session, const QVariantMap &row)
{
    if (&session != m_sessionService.currentSession()) {
        return;
    }

    session.runtime.logRows.append(row);
    trimVisibleRows(session, Stream::Log);
    m_logs.appendRow(row);
    m_logs.trimToLimit(kMaxVisibleEventRows);
    emit logAppended(row);
}

void EventHistoryService::appendEvent(SessionState &session, const QString &channel, const QString &message)
{
    const QString timestamp = timestampNow();
    const qint64 historyId = m_historyStore.appendEvent(session.id, timestamp, channel, message);
    m_historyStore.pruneLogs(session.id, m_preferencesController.logRetentionLimit());

    appendRenderedLogRow(session, EventRenderer::eventRow(historyId, timestamp, channel, message));
}

LuaScriptResult EventHistoryService::parseIncomingPayload(
    const SessionState &session,
    const SubscriptionEntry *subscription,
    const QString &topic,
    const QByteArray &payloadBytes,
    const QString &timestamp,
    QString &scriptNameOut)
{
    const PayloadFormat format = subscription
        ? PayloadCodec::formatFromInt(subscription->format)
        : PayloadCodec::resolveTopicFormat(session.runtime.subscriptionFormats, topic);

    QString decodeError;
    const QString decodedPayload = PayloadCodec::decodeForDisplay(format, payloadBytes, decodeError);

    if (!subscription || subscription->scriptId.isEmpty()) {
        return {};
    }

    const auto *script = m_scriptService.scriptById(subscription->scriptId);
    if (!script) {
        LuaScriptResult result;
        result.error = tr("Selected Lua script is missing.");
        return result;
    }

    scriptNameOut = script->name;

    LuaScriptContext context;
    context.topic = topic;
    context.payloadBytes = payloadBytes;
    context.decodedPayload = decodedPayload;
    context.decodeError = decodeError;
    context.format = format;
    context.timestamp = timestamp;
    return m_luaRuntimeCache.run(script->id, script->code, context);
}

void EventHistoryService::appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes)
{
    auto *session = m_sessionService.sessionById(sessionId);
    if (!session) {
        return;
    }

    const QString timestamp = timestampNow();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    appendRecentTrafficSample(
        session->runtime.recentReceivedTraffic,
        nowMs,
        payloadBytes.size());
    const bool isCurrentSession = session == m_sessionService.currentSession();
    const MessageSubscriptionMatch subscriptionMatch = matchSubscriptionsForMessage(*session, topic, nowMs, isCurrentSession);
    if (subscriptionMatch.currentSubscriptionActivity) {
        emit subscriptionActivityChanged();
    }

    if (session->outputPaused && !m_preferencesController.saveMessagesWhenOutputPaused()) {
        return;
    }

    const SubscriptionEntry *displaySubscription = subscriptionMatch.displaySubscription;
    const PayloadStoragePlan payloadPlan = makePayloadStoragePlan(
        topic,
        payloadBytes,
        m_preferencesController.maxIncomingPayloadBytes());
    if (payloadPlan.shouldReport) {
        const QString reportKey = QStringLiteral("%1|%2|%3").arg(session->id, topic, payloadPlan.state);
        if (!m_reportedPayloadStorageStates.contains(reportKey)) {
            m_reportedPayloadStorageStates.insert(reportKey);
            appendEvent(*session, QStringLiteral("Payload"), payloadPlan.reportMessage);
        }
    }

    QString scriptDisplayName;
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
            scriptDisplayName);
    } else if (hasScript) {
        scriptResult.error = QStringLiteral("Lua script skipped because payload exceeds the configured size limit.");
        scriptDisplayName = m_scriptService.scriptName(displaySubscription->scriptId);
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
    const qint64 historyId = m_historyStore.enqueueMessage(record);
    record.id = historyId;
    if (historyId <= 0) {
        reportMessageStorageError(
            *session,
            QStringLiteral("Cannot queue incoming message: %1").arg(m_historyStore.lastError()));
    } else {
        ++session->runtime.totalMessageCount;
        if (session == m_sessionService.currentSession()) {
            session->runtime.viewedMessageCount = session->runtime.totalMessageCount;
        }
        emit totalMessageCountChanged();
        m_lastMessageStorageError.clear();
        scheduleMessageHistoryFlush();
    }

    if (session != m_sessionService.currentSession() || session->outputPaused) {
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
    auto *session = m_sessionService.sessionById(sessionId);
    if (!session) {
        return;
    }

    const QString timestamp = timestampNow();
    const PayloadStoragePlan payloadPlan = makePayloadStoragePlan(
        topic,
        payloadBytes,
        m_preferencesController.maxIncomingPayloadBytes());
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
    const qint64 historyId = m_historyStore.enqueueMessage(record);
    record.id = historyId;
    if (historyId <= 0) {
        reportMessageStorageError(
            *session,
            QStringLiteral("Cannot queue published message: %1").arg(m_historyStore.lastError()));
    } else {
        ++session->runtime.totalMessageCount;
        if (session == m_sessionService.currentSession()) {
            session->runtime.viewedMessageCount = session->runtime.totalMessageCount;
        }
        emit totalMessageCountChanged();
        m_lastMessageStorageError.clear();
        scheduleMessageHistoryFlush();
    }

    if (session != m_sessionService.currentSession()) {
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

std::optional<QString> EventHistoryService::decodedStoredPayload(qint64 messageId, int format, QString &parseErrorOut) const
{
    if (messageId <= 0) {
        return std::nullopt;
    }

    const QByteArray payloadBytes = m_historyStore.loadMessagePayloadBytes(messageId);
    if (payloadBytes.isEmpty()) {
        return std::nullopt;
    }

    return PayloadCodec::decodeForDisplay(PayloadCodec::formatFromInt(format), payloadBytes, parseErrorOut);
}

QString EventHistoryService::messagePayloadForReuse(
    qint64 messageId,
    const QString &fallbackPayload,
    const QString &fallbackTestPayload,
    int format) const
{
    const QString fallback = fallbackTestPayload.isEmpty() ? fallbackPayload : fallbackTestPayload;
    QString parseError;
    const auto decoded = decodedStoredPayload(messageId, format, parseError);
    return decoded && parseError.isEmpty() ? *decoded : fallback;
}

QString EventHistoryService::messagePayloadForDisplay(
    qint64 messageId,
    const QString &fallbackPayload,
    int format) const
{
    QString parseError;
    const auto decoded = decodedStoredPayload(messageId, format, parseError);
    return decoded ? *decoded : fallbackPayload;
}

QVariantMap EventHistoryService::messageDetails(qint64 messageId) const
{
    if (messageId <= 0) {
        return {};
    }

    const QVariantMap stored = m_historyStore.loadMessage(messageId);
    if (stored.isEmpty()) {
        return {};
    }

    QHash<QString, int> formats;
    QHash<QString, QString> colors;
    QHash<QString, QString> aliases;
    if (const auto *session = m_sessionService.currentSession()) {
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

void EventHistoryService::trimVisibleRows(SessionState &session, Stream kind)
{
    auto &rows = streamRows(session, kind);
    const qsizetype overflow = rows.size() - kMaxVisibleEventRows;
    if (overflow <= 0) {
        return;
    }

    rows.remove(0, overflow);
    oldestLoadedId(session, kind) = EventRenderer::firstHistoryId(rows);
    loadedAllHistory(session, kind) = false;
}

void EventHistoryService::reloadCurrentSessionHistory()
{
    auto *session = m_sessionService.currentSession();
    if (!session) {
        return;
    }
    flushPendingVisibleMessageRows();
    flushPendingMessageHistory();
    m_messageStreamFrozen = false;
    m_frozenOldestLoadedMessageId = 0;

    const int pageSize = m_preferencesController.historyPageSize();
    const QVariantList messageRows = m_historyStore.loadMessages(session->id, pageSize);
    session->runtime.messageRows = EventRenderer::loadHistoryRows(
        messageRows,
        session->runtime.subscriptionFormats,
        subscriptionColors(*session),
        subscriptionAliases(*session),
        m_launchTimestamp,
        true);
    session->runtime.totalMessageCount = m_historyStore.totalMessageCount(session->id);
    session->runtime.viewedMessageCount = session->runtime.totalMessageCount;
    emit totalMessageCountChanged();
    session->runtime.oldestLoadedMessageId = EventRenderer::firstHistoryId(session->runtime.messageRows);
    session->runtime.loadedAllMessageHistory = messageRows.size() < pageSize;
    m_messages.setRows(session->runtime.messageRows);

    const QVariantList logRows = m_historyStore.loadLogs(session->id, pageSize);
    session->runtime.logRows = EventRenderer::loadHistoryRows(
        logRows,
        session->runtime.subscriptionFormats,
        {},
        {},
        m_launchTimestamp,
        true);
    session->runtime.oldestLoadedLogId = EventRenderer::firstHistoryId(session->runtime.logRows);
    session->runtime.loadedAllLogHistory = logRows.size() < pageSize;
    m_logs.setRows(session->runtime.logRows);
}

void EventHistoryService::flushPendingMessageHistory()
{
    if (m_historyStore.pendingMessageCount() <= 0) {
        return;
    }

    const QStringList flushedSessionIds = m_historyStore.flushPendingMessages();
    if (flushedSessionIds.isEmpty() && !m_historyStore.lastError().isEmpty()) {
        if (auto *session = m_sessionService.currentSession()) {
            reportMessageStorageError(
                *session,
                QStringLiteral("Cannot save queued messages: %1").arg(m_historyStore.lastError()));
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
    if (m_historyStore.pendingMessageCount() >= kMessageHistoryFlushBatchSize) {
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
