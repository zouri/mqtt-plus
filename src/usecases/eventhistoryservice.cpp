#include "eventhistoryservice.h"

#include "usecases/sessionservice.h"
#include "usecases/preferencescontroller.h"
#include "services/apputils.h"
#include "models/eventstreammodel.h"
#include "presentation/eventrenderer.h"
#include "services/payload/payloadcodec.h"
#include "services/parsing/messageparseworker.h"
#include "services/processors/processorlibrary.h"
#include "services/storage/historystore.h"
#include "services/storage/historywriterworker.h"
#include "domain/messagerecord.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QHash>
#include <QMetaObject>

#include <algorithm>

using namespace AppUtils;

namespace {
constexpr int kVisibleMessageRowsFlushIntervalMs = 16;
constexpr int kMessagePressureNotificationIntervalMs = 100;
constexpr qint64 kPayloadPreviewBytes = 64 * 1024;
constexpr qint64 kPressurePayloadPreviewBytes = 4 * 1024;
constexpr qsizetype kVisibleParsedCharacters = 64 * 1024;
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

bool requiresBackgroundParse(PayloadFormat format)
{
    return format == PayloadFormat::Json
        || format == PayloadFormat::Cbor
        || format == PayloadFormat::MsgPack;
}

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

bool looksBinary(const QByteArray &bytes, qsizetype sampleLimit)
{
    if (bytes.isEmpty()) {
        return false;
    }

    const qsizetype sampleSize = (std::min)(bytes.size(), sampleLimit);
    qsizetype suspicious = 0;
    for (qsizetype i = 0; i < sampleSize; ++i) {
        const uchar ch = static_cast<uchar>(bytes.at(i));
        if (ch == 0 || (ch < 0x20 && ch != '\n' && ch != '\r' && ch != '\t')) {
            ++suspicious;
        }
    }
    return suspicious > 0 || suspicious * 100 > sampleSize * 15;
}

QString payloadTextPreview(const QByteArray &bytes, qint64 previewLimit)
{
    const QByteArray previewBytes = bytes.left((std::min)(bytes.size(), qsizetype(previewLimit)));
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

PayloadStoragePlan makePayloadStoragePlan(
    const QString &topic,
    const QByteArray &payloadBytes,
    int configuredLimit,
    bool compactPreview = false)
{
    PayloadStoragePlan plan;
    plan.originalSize = payloadBytes.size();

    const qint64 maxBytes = configuredLimit > 0 ? configuredLimit : kHardPayloadLimitBytes;
    const qint64 previewLimit = compactPreview
        ? kPressurePayloadPreviewBytes
        : kPayloadPreviewBytes;
    const bool binary = looksBinary(
        payloadBytes,
        compactPreview ? qsizetype(1024) : qsizetype(4096));
    plan.preview = binary
        ? payloadHexPreview(payloadBytes)
        : payloadTextPreview(payloadBytes, previewLimit);

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
    row.insert(QStringLiteral("display_payload"), record.displayPayload);
    row.insert(QStringLiteral("display_format"), record.displayFormat);
    row.insert(QStringLiteral("display_error"), record.displayError);
    row.insert(QStringLiteral("display_state"), record.displayState);
    row.insert(QStringLiteral("processor_id"), record.processorId);
    row.insert(QStringLiteral("processor_revision_id"), record.processorRevisionId);
    row.insert(QStringLiteral("processor_name"), record.processorName);
    row.insert(QStringLiteral("processor_language_id"), record.processorLanguageId);
    row.insert(QStringLiteral("processor_runtime_id"), record.processorRuntimeId);
    row.insert(QStringLiteral("processor_content_hash"), record.processorContentHash);
    row.insert(QStringLiteral("processor_result_preview"), record.processorResultPreview);
    row.insert(QStringLiteral("processor_execution_state"), record.processorExecutionState);
    row.insert(QStringLiteral("processor_execution_error_code"), record.processorExecutionErrorCode);
    row.insert(QStringLiteral("processor_execution_error"), record.processorExecutionError);
    row.insert(QStringLiteral("processor_execution_duration_us"), record.processorExecutionDurationUs);
    return row;
}

void applyParseResultToStorageRow(QVariantMap &row, const MessageParseResult &result)
{
    row.insert(QStringLiteral("display_payload"), result.displayPayload);
    row.insert(QStringLiteral("display_format"), result.displayFormat);
    row.insert(QStringLiteral("display_error"), result.displayError);
    row.insert(QStringLiteral("display_state"), messageParseStateName(result.state));
    row.insert(QStringLiteral("processor_id"), result.processorId);
    row.insert(QStringLiteral("processor_revision_id"), result.processorRevisionId);
    row.insert(QStringLiteral("processor_name"), result.processorName);
    row.insert(QStringLiteral("processor_language_id"), result.processorLanguageId);
    row.insert(QStringLiteral("processor_runtime_id"), result.processorRuntimeId);
    row.insert(QStringLiteral("processor_content_hash"), result.processorContentHash);
    row.insert(QStringLiteral("processor_result_cbor"), result.processorResultCbor);
    row.insert(QStringLiteral("processor_result_preview"), result.processorResultPreview);
    row.insert(QStringLiteral("processor_execution_state"), result.processorExecutionState);
    row.insert(QStringLiteral("processor_execution_error_code"), result.processorExecutionErrorCode);
    row.insert(QStringLiteral("processor_execution_error"), result.processorExecutionError);
    row.insert(QStringLiteral("processor_execution_duration_us"), result.processorExecutionDurationUs);
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
            appendRecentMessage(subscription.recentMessages, nowMs);
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
    HistoryWriterWorker &historyWriter,
    MessageParseWorker &messageParser,
    EventStreamModel &messages,
    EventStreamModel &logs,
    ProcessorLibrary &processorLibrary,
    QString launchTimestamp,
    PreferencesController &preferencesController,
    QObject *parent)
    : QObject(parent)
    , m_sessionService(sessionService)
    , m_historyStore(historyStore)
    , m_historyWriter(historyWriter)
    , m_messageParser(messageParser)
    , m_messages(messages)
    , m_logs(logs)
    , m_processorLibrary(processorLibrary)
    , m_launchTimestamp(std::move(launchTimestamp))
    , m_preferencesController(preferencesController)
{
    m_visibleMessageRowsFlushTimer.setInterval(kVisibleMessageRowsFlushIntervalMs);
    m_visibleMessageRowsFlushTimer.setSingleShot(true);
    m_messagePressureNotificationTimer.setInterval(kMessagePressureNotificationIntervalMs);
    m_messagePressureNotificationTimer.setSingleShot(true);
    connect(
        &m_historyWriter,
        &HistoryWriterWorker::queueStateChanged,
        this,
        &EventHistoryService::scheduleMessagePressureNotification,
        Qt::UniqueConnection);
    connect(
        &m_historyWriter,
        &HistoryWriterWorker::storageErrorChanged,
        this,
        [this](const QString &error) {
            if (error.isEmpty()) {
                m_lastMessageStorageError.clear();
            } else {
                m_lastMessageStorageError = QStringLiteral("Cannot save queued messages: %1").arg(error);
            }
            scheduleMessagePressureNotification();
        });
    connect(
        &m_visibleMessageRowsFlushTimer,
        &QTimer::timeout,
        this,
        &EventHistoryService::flushPendingVisibleMessageRows,
        Qt::UniqueConnection);
    connect(
        &m_messagePressureNotificationTimer,
        &QTimer::timeout,
        this,
        &EventHistoryService::messageWriterStateChanged,
        Qt::UniqueConnection);
    connect(
        &m_messageParser,
        &MessageParseWorker::parseCompleted,
        this,
        [this](const MessageParseResult &result) {
            if (!m_historyWriter.enqueueParseResult(result)) {
                return;
            }
            MessageParseResult visibleResult = result;
            visibleResult.processorResultCbor.clear();
            QMetaObject::invokeMethod(
                this,
                [this, visibleResult]() { handleMessageParseResult(visibleResult); },
                Qt::QueuedConnection);
        },
        Qt::DirectConnection);
    connect(
        &m_messageParser,
        &MessageParseWorker::queueStateChanged,
        this,
        &EventHistoryService::scheduleMessagePressureNotification,
        Qt::QueuedConnection);
}

bool EventHistoryService::clearStream(Stream kind, bool allSessions)
{
    auto *current = m_sessionService.currentSession();
    const bool isMessage = kind == Stream::Message;

    if (isMessage && !flushPendingMessageHistory()) {
        if (current) {
            reportMessageStorageError(
                *current,
                QStringLiteral("Cannot drain queued messages before clearing history: %1")
                    .arg(m_historyWriter.lastError()));
        }
        return false;
    }

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
    if (!flushPendingMessageHistory()) {
        if (current) {
            reportMessageStorageError(
                *current,
                QStringLiteral("Cannot drain queued messages before clearing history: %1")
                    .arg(m_historyWriter.lastError()));
        }
        return false;
    }
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
    if (rows.size() > kMaxVisibleEventRows) {
        rows.remove(0, rows.size() - kMaxVisibleEventRows);
    }

    auto &model = isMessage ? m_messages : m_logs;
    if (frozen) {
        m_frozenOldestLoadedMessageId = EventRenderer::firstHistoryId(rows);
        return model.prependRowsAndTrimBack(rows, kMaxVisibleEventRows);
    }

    QVariantList merged = rows;
    merged.append(currentRows);
    if (merged.size() > kMaxVisibleEventRows) {
        merged.resize(kMaxVisibleEventRows);
    }
    currentRows = merged;
    oldestLoadedId(*session, kind) = EventRenderer::firstHistoryId(currentRows);
    return model.prependRowsAndTrimBack(rows, kMaxVisibleEventRows);
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
        const bool pendingRowsAlreadyReflected = m_messages.rowCount()
                == currentSession->runtime.messageRows.size()
            && m_messages.lastRowEquals(rows.constLast().toMap());
        if (!pendingRowsAlreadyReflected) {
            m_messages.appendRowsAndTrimFront(rows, kMaxVisibleEventRows);
        }
    }

    if (!rows.isEmpty()) {
        emit messageRowsAppended(rows);
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

void EventHistoryService::enqueueMessageParsing(
    const MessageRecord &record,
    qint64 sequence,
    const QSharedPointer<const ProcessorRevisionSnapshot> &processorRevision,
    const QCborMap &processorParameters)
{
    MessageParseTask task;
    task.envelope.messageId = record.id;
    task.envelope.sequence = sequence;
    task.envelope.sessionId = record.sessionId;
    task.envelope.timestamp = record.timestamp;
    task.envelope.topic = record.topic;
    task.envelope.payloadBytes = record.payloadBytes;
    task.envelope.payloadFormat = record.payloadFormat;
    task.processorRevision = processorRevision;
    task.processorName = record.processorName;
    task.processorParameters = processorParameters;
    if (m_messageParser.enqueueTask(task)) {
        return;
    }

    MessageParseResult result;
    result.messageId = record.id;
    result.sequence = sequence;
    result.sessionId = record.sessionId;
    result.processorId = record.processorId;
    result.processorRevisionId = record.processorRevisionId;
    result.processorName = record.processorName;
    result.processorLanguageId = record.processorLanguageId;
    result.processorRuntimeId = record.processorRuntimeId;
    result.processorContentHash = record.processorContentHash;
    result.state = MessageParseState::SkippedOverload;
    result.displayFormat = record.processorId.isEmpty()
        ? QStringLiteral("Parse skipped")
        : QStringLiteral("Processor skipped");
    result.displayError = QStringLiteral("Parser skipped because its bounded queue is full.");
    if (!record.processorId.isEmpty()) {
        result.processorExecutionState = QStringLiteral("skipped_overload");
        result.processorExecutionErrorCode = QStringLiteral("processor_queue_overloaded");
        result.processorExecutionError = result.displayError;
    }
    if (m_historyWriter.enqueueParseResult(result)) {
        handleMessageParseResult(result);
    }
}

void EventHistoryService::handleMessageParseResult(const MessageParseResult &result)
{
    auto *session = m_sessionService.sessionById(result.sessionId);
    if (!session) {
        return;
    }
    updateRenderedParseResult(*session, result);
    emit messageParseResultChanged(result.messageId);
}

void EventHistoryService::updateRenderedParseResult(
    SessionState &session,
    const MessageParseResult &result)
{
    auto applyResult = [&result](QVariantMap &row) {
        if (row.value(QStringLiteral("historyId")).toLongLong() != result.messageId) {
            return false;
        }

        const QString visibleParsedPayload = result.displayPayload.left(kVisibleParsedCharacters);
        row.insert(QStringLiteral("parseState"), messageParseStateName(result.state));
        row.insert(QStringLiteral("parsedPayload"), visibleParsedPayload);
        if (result.state == MessageParseState::Succeeded) {
            row.insert(QStringLiteral("payload"), visibleParsedPayload);
            row.insert(QStringLiteral("payloadFormat"), result.displayFormat);
            if (result.processorId.isEmpty()) {
                row.insert(QStringLiteral("testPayload"), visibleParsedPayload);
            }
        } else if (result.state == MessageParseState::Failed) {
            const QString errorLabel = result.processorId.isEmpty()
                ? QStringLiteral("Parser Error")
                : QStringLiteral("Processor Error");
            const QString basePayload = row.value(QStringLiteral("testPayload")).toString();
            row.insert(
                QStringLiteral("payload"),
                basePayload.isEmpty()
                    ? QStringLiteral("%1: %2").arg(errorLabel, result.displayError)
                    : QStringLiteral("%1\n%2: %3").arg(basePayload, errorLabel, result.displayError));
            row.insert(
                QStringLiteral("payloadFormat"),
                result.processorId.isEmpty()
                    ? (result.displayFormat.isEmpty()
                        ? QStringLiteral("Parser Error")
                        : QStringLiteral("%1 Error").arg(result.displayFormat))
                    : QStringLiteral("Processor Error"));
        } else if (result.state == MessageParseState::SkippedOverload) {
            row.insert(QStringLiteral("payloadFormat"), QStringLiteral("Parse skipped"));
        }
        return true;
    };

    QVariantMap updatedRow;
    for (auto item = session.runtime.messageRows.rbegin();
         item != session.runtime.messageRows.rend();
         ++item) {
        QVariantMap row = item->toMap();
        if (applyResult(row)) {
            *item = row;
            updatedRow = row;
            break;
        }
    }
    if (updatedRow.isEmpty()) {
        return;
    }

    bool pendingRowUpdated = false;
    if (m_pendingVisibleMessageSessionId == session.id) {
        for (auto item = m_pendingVisibleMessageRows.rbegin();
             item != m_pendingVisibleMessageRows.rend();
             ++item) {
            QVariantMap row = item->toMap();
            if (applyResult(row)) {
                *item = row;
                pendingRowUpdated = true;
                break;
            }
        }
    }

    if (&session == m_sessionService.currentSession()
        && !m_messageStreamFrozen
        && !pendingRowUpdated) {
        m_messages.updateRowByHistoryId(result.messageId, updatedRow);
    }
}

void EventHistoryService::appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes)
{
    auto *session = m_sessionService.sessionById(sessionId);
    if (!session) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    appendRecentTrafficSample(
        session->runtime.recentReceivedTraffic,
        nowMs,
        payloadBytes.size());
    if (!shouldCaptureMessage(*session, MessageDirection::Incoming, topic)
        || (session->outputPaused && !m_preferencesController.saveMessagesWhenOutputPaused())) {
        recordCaptureFiltered();
        return;
    }

    const bool isCurrentSession = session == m_sessionService.currentSession();
    const bool pressureSkipsParsing = shouldSkipParsingForPressure();
    const MessageSubscriptionMatch subscriptionMatch = matchSubscriptionsForMessage(*session, topic, nowMs, isCurrentSession);
    if (subscriptionMatch.currentSubscriptionActivity) {
        emit subscriptionActivityChanged();
    }

    const QString timestamp = timestampNow();
    const SubscriptionEntry *displaySubscription = subscriptionMatch.displaySubscription;
    const PayloadStoragePlan payloadPlan = makePayloadStoragePlan(
        topic,
        payloadBytes,
        m_preferencesController.maxIncomingPayloadBytes(),
        pressureSkipsParsing);
    if (payloadPlan.shouldReport) {
        const QString reportKey = QStringLiteral("%1|%2|%3").arg(session->id, topic, payloadPlan.state);
        if (!m_reportedPayloadStorageStates.contains(reportKey)) {
            m_reportedPayloadStorageStates.insert(reportKey);
            appendEvent(*session, QStringLiteral("Payload"), payloadPlan.reportMessage);
        }
    }

    const ProcessorReference *processorReference = displaySubscription
            && !displaySubscription->processor.processorId.isEmpty()
        ? &displaySubscription->processor
        : nullptr;
    std::optional<ResolvedProcessor> resolvedProcessor;
    QString processorResolutionError;
    if (processorReference) {
        resolvedProcessor = m_processorLibrary.resolve(
            *processorReference,
            &processorResolutionError);
    }
    const int payloadFormat = subscriptionMatch.payloadFormat >= 0
        ? subscriptionMatch.payloadFormat
        : static_cast<int>(PayloadCodec::resolveTopicFormat(
            session->runtime.subscriptionFormats,
            topic));
    bool parsingRequired = requiresBackgroundParse(PayloadCodec::formatFromInt(payloadFormat))
        || processorReference;
    bool parsingSkippedForPressure = false;
    QSharedPointer<const ProcessorRevisionSnapshot> processorRevision;
    QCborMap processorParameters;

    MessageRecord record;
    record.sessionId = sessionId;
    record.timestamp = timestamp;
    record.direction = MessageDirection::Incoming;
    record.topic = topic;
    record.payloadBytes = payloadPlan.storedBytes;
    record.payloadPreview = payloadPlan.preview;
    record.payloadState = payloadPlan.state;
    record.payloadSize = payloadPlan.originalSize;
    record.payloadHash = payloadPlan.hash;
    record.payloadFormat = payloadFormat;
    if (processorReference) {
        record.processorId = processorReference->processorId;
        processorParameters = processorReference->parameters;
        if (resolvedProcessor) {
            processorRevision = resolvedProcessor->revision;
            record.processorRevisionId = resolvedProcessor->revision->id;
            record.processorName = resolvedProcessor->processor.name;
            record.processorLanguageId = resolvedProcessor->revision->languageId;
            record.processorRuntimeId = resolvedProcessor->revision->runtimeId;
            record.processorContentHash = resolvedProcessor->revision->contentHash;
        } else if (const auto processor = m_processorLibrary.processorById(record.processorId)) {
            record.processorName = processor->name;
        }
    }
    if (!parsingRequired) {
        record.displayState = messageParseStateName(MessageParseState::NotRequired);
    } else if (processorReference && !resolvedProcessor) {
        record.displayState = messageParseStateName(MessageParseState::Failed);
        record.displayError = processorResolutionError.isEmpty()
            ? QStringLiteral("Selected Processor is unavailable.")
            : processorResolutionError;
        record.displayFormat = QStringLiteral("Processor Error");
        record.processorExecutionState = QStringLiteral("preparation_failed");
        record.processorExecutionErrorCode = QStringLiteral("processor_not_found");
        record.processorExecutionError = record.displayError;
        parsingRequired = false;
    } else if (!payloadPlan.allowFullProcessing) {
        record.displayState = messageParseStateName(MessageParseState::Failed);
        record.displayError = processorReference
            ? QStringLiteral("Processor skipped because the payload exceeds the configured size limit.")
            : QStringLiteral("Payload parsing skipped because the payload exceeds the configured size limit.");
        record.displayFormat = processorReference
            ? QStringLiteral("Processor Error")
            : PayloadCodec::formatName(PayloadCodec::formatFromInt(payloadFormat));
        if (processorReference) {
            record.processorExecutionState = QStringLiteral("skipped_payload_limit");
            record.processorExecutionErrorCode = QStringLiteral("payload_too_large");
            record.processorExecutionError = record.displayError;
        }
        parsingRequired = false;
    } else if (pressureSkipsParsing) {
        record.displayState = messageParseStateName(MessageParseState::SkippedOverload);
        record.displayError = QStringLiteral(
            "Payload parsing skipped while the capture pipeline is under pressure.");
        record.displayFormat = processorReference
            ? QStringLiteral("Processor skipped")
            : PayloadCodec::formatName(PayloadCodec::formatFromInt(payloadFormat));
        if (processorReference) {
            record.processorExecutionState = QStringLiteral("skipped_overload");
            record.processorExecutionErrorCode = QStringLiteral("capture_pipeline_overloaded");
            record.processorExecutionError = record.displayError;
        }
        parsingRequired = false;
        parsingSkippedForPressure = true;
    } else {
        record.displayState = messageParseStateName(MessageParseState::Pending);
        if (processorReference) {
            record.processorExecutionState = QStringLiteral("pending");
        }
    }

    const qint64 sequence = m_nextMessageSequence.value(sessionId) + 1;
    const qint64 historyId = m_historyWriter.enqueueMessage(record);
    record.id = historyId;
    if (historyId <= 0) {
        return;
    } else {
        m_nextMessageSequence.insert(sessionId, sequence);
        if (parsingSkippedForPressure) {
            recordPressureSkippedParse();
        }
        ++session->runtime.totalMessageCount;
        if (session == m_sessionService.currentSession()) {
            session->runtime.viewedMessageCount = session->runtime.totalMessageCount;
        }
        emit totalMessageCountChanged();
        m_lastMessageStorageError.clear();
    }

    if (session == m_sessionService.currentSession() && !session->outputPaused) {
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

    if (parsingRequired) {
        enqueueMessageParsing(record, sequence, processorRevision, processorParameters);
    }
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

    if (!shouldCaptureMessage(*session, MessageDirection::Outgoing, topic)) {
        recordCaptureFiltered();
        return;
    }

    const bool pressureSkipsParsing = shouldSkipParsingForPressure();
    const QString timestamp = timestampNow();
    const PayloadStoragePlan payloadPlan = makePayloadStoragePlan(
        topic,
        payloadBytes,
        m_preferencesController.maxIncomingPayloadBytes(),
        pressureSkipsParsing);
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
    bool parsingRequired = requiresBackgroundParse(PayloadCodec::formatFromInt(format));
    bool parsingSkippedForPressure = false;
    if (!parsingRequired) {
        record.displayState = messageParseStateName(MessageParseState::NotRequired);
    } else if (!payloadPlan.allowFullProcessing) {
        record.displayState = messageParseStateName(MessageParseState::Failed);
        record.displayError = QStringLiteral(
            "Payload parsing skipped because the payload exceeds the configured size limit.");
        record.displayFormat = PayloadCodec::formatName(PayloadCodec::formatFromInt(format));
        parsingRequired = false;
    } else if (pressureSkipsParsing) {
        record.displayState = messageParseStateName(MessageParseState::SkippedOverload);
        record.displayError = QStringLiteral(
            "Payload parsing skipped while the capture pipeline is under pressure.");
        record.displayFormat = PayloadCodec::formatName(PayloadCodec::formatFromInt(format));
        parsingRequired = false;
        parsingSkippedForPressure = true;
    } else {
        record.displayState = messageParseStateName(MessageParseState::Pending);
    }

    const qint64 sequence = m_nextMessageSequence.value(sessionId) + 1;
    const qint64 historyId = m_historyWriter.enqueueMessage(record);
    record.id = historyId;
    if (historyId <= 0) {
        return;
    } else {
        m_nextMessageSequence.insert(sessionId, sequence);
        if (parsingSkippedForPressure) {
            recordPressureSkippedParse();
        }
        ++session->runtime.totalMessageCount;
        if (session == m_sessionService.currentSession()) {
            session->runtime.viewedMessageCount = session->runtime.totalMessageCount;
        }
        emit totalMessageCountChanged();
        m_lastMessageStorageError.clear();
    }

    if (session == m_sessionService.currentSession()) {
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

    if (parsingRequired) {
        enqueueMessageParsing(record, sequence);
    }
}

std::optional<QString> EventHistoryService::decodedStoredPayload(qint64 messageId, int format, QString &parseErrorOut) const
{
    if (messageId <= 0) {
        return std::nullopt;
    }

    QByteArray payloadBytes;
    if (const auto pending = m_historyWriter.pendingMessage(messageId)) {
        payloadBytes = pending->payloadBytes;
    } else {
        payloadBytes = m_historyStore.loadMessagePayloadBytes(messageId);
    }
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

    QVariantMap stored;
    if (const auto pending = m_historyWriter.pendingMessage(messageId)) {
        stored = messageRecordRow(*pending);
    } else {
        stored = m_historyStore.loadMessage(messageId);
        if (const auto pendingResult = m_historyWriter.pendingParseResult(messageId)) {
            applyParseResultToStorageRow(stored, *pendingResult);
        }
    }
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
    details.insert(
        QStringLiteral("payloadFormat"),
        PayloadCodec::formatName(PayloadCodec::formatFromInt(
            stored.value(QStringLiteral("payload_format"), -1).toInt())));
    details.insert(QStringLiteral("parsedPayload"), stored.value(QStringLiteral("display_payload")));
    details.insert(QStringLiteral("parseError"), stored.value(QStringLiteral("display_error")));
    details.insert(QStringLiteral("parseState"), stored.value(QStringLiteral("display_state")));
    details.insert(QStringLiteral("processorId"), stored.value(QStringLiteral("processor_id")));
    details.insert(
        QStringLiteral("processorRevisionId"),
        stored.value(QStringLiteral("processor_revision_id")));
    details.insert(QStringLiteral("processorName"), stored.value(QStringLiteral("processor_name")));
    details.insert(
        QStringLiteral("processorExecutionState"),
        stored.value(QStringLiteral("processor_execution_state")));
    details.insert(
        QStringLiteral("processorExecutionErrorCode"),
        stored.value(QStringLiteral("processor_execution_error_code")));
    details.insert(
        QStringLiteral("processorExecutionError"),
        stored.value(QStringLiteral("processor_execution_error")));
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
    m_messageStreamFrozen = false;
    m_frozenOldestLoadedMessageId = 0;

    const int pageSize = m_preferencesController.historyPageSize();
    const QVector<MessageRecord> pendingMessages = m_historyWriter.pendingMessages(session->id);
    const QVector<MessageParseResult> pendingParseResults =
        m_historyWriter.pendingParseResults(session->id);
    QVariantList messageRows = m_historyStore.loadMessages(session->id, pageSize);
    const bool loadedAllPersistedMessageHistory = messageRows.size() < pageSize;
    QMap<qint64, QVariantMap> messageRowsById;
    for (const QVariant &rowValue : std::as_const(messageRows)) {
        const QVariantMap row = rowValue.toMap();
        messageRowsById.insert(row.value(QStringLiteral("id")).toLongLong(), row);
    }
    for (const MessageRecord &message : pendingMessages) {
        messageRowsById.insert(message.id, messageRecordRow(message));
    }
    for (const MessageParseResult &parseResult : pendingParseResults) {
        auto row = messageRowsById.find(parseResult.messageId);
        if (row != messageRowsById.end()) {
            applyParseResultToStorageRow(row.value(), parseResult);
        }
    }
    while (messageRowsById.size() > pageSize) {
        messageRowsById.erase(messageRowsById.begin());
    }
    messageRows.clear();
    for (const QVariantMap &row : std::as_const(messageRowsById)) {
        messageRows.append(row);
    }
    session->runtime.messageRows = EventRenderer::loadHistoryRows(
        messageRows,
        session->runtime.subscriptionFormats,
        subscriptionColors(*session),
        subscriptionAliases(*session),
        m_launchTimestamp,
        true);
    const qint64 persistedTotal = m_historyStore.totalMessageCount(session->id);
    if (session->runtime.totalMessageCount <= 0) {
        session->runtime.totalMessageCount = persistedTotal + pendingMessages.size();
    } else {
        session->runtime.totalMessageCount = (std::max)(
            session->runtime.totalMessageCount,
            persistedTotal);
    }
    session->runtime.viewedMessageCount = session->runtime.totalMessageCount;
    emit totalMessageCountChanged();
    session->runtime.oldestLoadedMessageId = EventRenderer::firstHistoryId(session->runtime.messageRows);
    session->runtime.loadedAllMessageHistory = loadedAllPersistedMessageHistory;
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

bool EventHistoryService::flushPendingMessageHistory(int timeoutMs)
{
    const bool parserDrained = m_messageParser.drain(timeoutMs);
    bool writerDrained = true;

    if (m_historyWriter.pendingMessageCount() > 0
        && !m_historyWriter.drain(timeoutMs)) {
        writerDrained = false;
    }

    if (!writerDrained) {
        const QString error = m_historyWriter.lastError().isEmpty()
            ? tr("Timed out while saving queued messages.")
            : m_historyWriter.lastError();
        m_lastMessageStorageError = QStringLiteral("Cannot save queued messages: %1").arg(error);
        return false;
    }

    if (!parserDrained) {
        m_lastMessageStorageError = QStringLiteral("Timed out while parsing queued messages.");
        return false;
    }

    m_lastMessageStorageError.clear();
    return true;
}

void EventHistoryService::stopAcceptingMessageParsing()
{
    m_messageParser.stopAccepting();
}

int EventHistoryService::messageWriterBacklog() const
{
    return m_historyWriter.pendingMessageCount();
}

qint64 EventHistoryService::messageWriterBacklogBytes() const
{
    return m_historyWriter.pendingBytes();
}

qint64 EventHistoryService::droppedMessageCount() const
{
    return m_historyWriter.droppedMessageCount();
}

qint64 EventHistoryService::droppedParseResultCount() const
{
    return m_historyWriter.droppedParseResultCount();
}

int EventHistoryService::messageParserBacklog() const
{
    return m_messageParser.pendingTaskCount();
}

qint64 EventHistoryService::messageParserBacklogBytes() const
{
    return m_messageParser.pendingBytes();
}

qint64 EventHistoryService::droppedParseTaskCount() const
{
    return m_messageParser.droppedTaskCount();
}

bool EventHistoryService::setMessageCapturePolicy(
    const QString &sessionId,
    const MessageCapturePolicy &policy)
{
    if (sessionId.isEmpty()) {
        return false;
    }
    if (!m_sessionService.setMessageCapturePolicy(sessionId, policy)) {
        return false;
    }
    scheduleMessagePressureNotification();
    return true;
}

MessageCapturePolicy EventHistoryService::messageCapturePolicy(const QString &sessionId) const
{
    return m_sessionService.messageCapturePolicy(sessionId);
}

qint64 EventHistoryService::captureFilteredMessageCount() const
{
    return m_captureFilteredMessages;
}

qint64 EventHistoryService::pressureSkippedParseCount() const
{
    return m_pressureSkippedParses;
}

QString EventHistoryService::messagePressureState() const
{
    const auto writerState = m_historyWriter.pressureState();
    const auto parserState = m_messageParser.pressureState();
    if (writerState == HistoryWriterWorker::PressureState::Dropping) {
        return QStringLiteral("dropping");
    }
    if (writerState == HistoryWriterWorker::PressureState::Degraded
        || parserState == MessageParseWorker::PressureState::Dropping) {
        return QStringLiteral("degraded");
    }
    if (writerState == HistoryWriterWorker::PressureState::Elevated
        || parserState == MessageParseWorker::PressureState::Elevated) {
        return QStringLiteral("elevated");
    }
    return QStringLiteral("normal");
}

QString EventHistoryService::messageCaptureMode() const
{
    const QString pressureState = messagePressureState();
    if (pressureState == QStringLiteral("normal")) {
        return QStringLiteral("full");
    }
    if (pressureState == QStringLiteral("dropping")) {
        return QStringLiteral("dropping");
    }
    return QStringLiteral("raw_only");
}

QString EventHistoryService::messageWriterPressureState() const
{
    switch (m_historyWriter.pressureState()) {
    case HistoryWriterWorker::PressureState::Elevated:
        return QStringLiteral("elevated");
    case HistoryWriterWorker::PressureState::Degraded:
        return QStringLiteral("degraded");
    case HistoryWriterWorker::PressureState::Dropping:
        return QStringLiteral("dropping");
    case HistoryWriterWorker::PressureState::Normal:
        return QStringLiteral("normal");
    }
    return QStringLiteral("normal");
}

QString EventHistoryService::messageParserPressureState() const
{
    switch (m_messageParser.pressureState()) {
    case MessageParseWorker::PressureState::Elevated:
        return QStringLiteral("elevated");
    case MessageParseWorker::PressureState::Dropping:
        return QStringLiteral("dropping");
    case MessageParseWorker::PressureState::Normal:
        return QStringLiteral("normal");
    }
    return QStringLiteral("normal");
}

bool EventHistoryService::messageStorageDegraded() const
{
    return m_historyWriter.pressureState() == HistoryWriterWorker::PressureState::Degraded
        || !m_historyWriter.lastError().isEmpty();
}

QString EventHistoryService::messageStorageError() const
{
    const QString error = m_historyWriter.lastError();
    return error.isEmpty() ? m_lastMessageStorageError : error;
}

void EventHistoryService::reportMessageStorageError(SessionState &session, const QString &message)
{
    if (message == m_lastMessageStorageError) {
        return;
    }

    m_lastMessageStorageError = message;
    appendEvent(session, QStringLiteral("Storage"), message);
}

void EventHistoryService::scheduleVisibleMessageRowsFlush()
{
    if (!m_visibleMessageRowsFlushTimer.isActive()) {
        m_visibleMessageRowsFlushTimer.start();
    }
}

void EventHistoryService::scheduleMessagePressureNotification()
{
    if (!m_messagePressureNotificationTimer.isActive()) {
        m_messagePressureNotificationTimer.start();
    }
}

bool EventHistoryService::shouldCaptureMessage(
    const SessionState &session,
    MessageDirection direction,
    const QString &topic) const
{
    return m_sessionService.messageCapturePolicy(session.id).accepts(direction, topic);
}

bool EventHistoryService::shouldSkipParsingForPressure() const
{
    return m_historyWriter.pressureState() != HistoryWriterWorker::PressureState::Normal
        || m_messageParser.pressureState() != MessageParseWorker::PressureState::Normal;
}

void EventHistoryService::recordCaptureFiltered()
{
    ++m_captureFilteredMessages;
    scheduleMessagePressureNotification();
}

void EventHistoryService::recordPressureSkippedParse()
{
    ++m_pressureSkippedParses;
    scheduleMessagePressureNotification();
}
