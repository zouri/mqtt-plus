#include "eventhistoryservice.h"

#include "usecases/sessionservice.h"
#include "usecases/preferencescontroller.h"
#include "services/apputils.h"
#include "models/eventstreammodel.h"
#include "presentation/eventrenderer.h"
#include "services/payload/payloadcodec.h"
#include "services/messaging/messageadmissionworker.h"
#include "services/messaging/messagepayloadplan.h"
#include "services/parsing/messageparseworker.h"
#include "services/processors/processorlibrary.h"
#include "services/storage/historystore.h"
#include "services/storage/historywriterworker.h"
#include "domain/messagerecord.h"

#include <QDateTime>
#include <QHash>
#include <QMetaObject>

#include <algorithm>

using namespace AppUtils;

struct SubscriptionRenderContext
{
    QHash<QString, int> formats;
    QHash<QString, QString> colors;
    QHash<QString, QString> aliases;
};

namespace {
constexpr int kVisibleMessageRowsFlushIntervalMs = 16;
constexpr int kMessagePressureNotificationIntervalMs = 100;
constexpr int kMessageActivityNotificationIntervalMs = 50;
constexpr qsizetype kVisibleParsedCharacters = 64 * 1024;
}

EventHistoryService::VisibleStreamState &EventHistoryService::visibleStreamState(
    const SessionState &session)
{
    if (m_visibleStreamSessionId != session.id) {
        m_visibleStreamState = {};
        m_visibleStreamSessionId = session.id;
    }
    return m_visibleStreamState;
}

QVector<EventRow> &EventHistoryService::streamRows(const SessionState &session, Stream kind)
{
    VisibleStreamState &state = visibleStreamState(session);
    return kind == Stream::Message ? state.messageRows : state.logRows;
}

qint64 &EventHistoryService::oldestLoadedId(const SessionState &session, Stream kind)
{
    VisibleStreamState &state = visibleStreamState(session);
    return kind == Stream::Message
        ? state.oldestLoadedMessageId
        : state.oldestLoadedLogId;
}

bool &EventHistoryService::loadedAllHistory(const SessionState &session, Stream kind)
{
    VisibleStreamState &state = visibleStreamState(session);
    return kind == Stream::Message
        ? state.loadedAllMessageHistory
        : state.loadedAllLogHistory;
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
    m_messageActivityNotificationTimer.setInterval(kMessageActivityNotificationIntervalMs);
    m_messageActivityNotificationTimer.setSingleShot(true);
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
        &m_historyWriter,
        &HistoryWriterWorker::expandedMessageLoaded,
        this,
        [this](qint64 messageId, const QString &payload, const QString &state) {
            m_messages.finishExpandedPayloadLoad(messageId, payload, state);
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
        &m_messageActivityNotificationTimer,
        &QTimer::timeout,
        this,
        &EventHistoryService::flushPendingMessageActivityNotifications,
        Qt::UniqueConnection);
    connect(
        &m_messageParser,
        &MessageParseWorker::parseCompleted,
        this,
        [this](const ParseOutcome &result) {
            if (!m_historyWriter.enqueueParseResult(result)) {
                return;
            }
            ParseOutcome visibleResult = result;
            visibleResult.processorResultCbor.clear();
            QMetaObject::invokeMethod(
                this,
                [this, visibleResult]() { handleParseOutcome(visibleResult); },
                Qt::QueuedConnection);
        },
        Qt::DirectConnection);
    connect(
        &m_messageParser,
        &MessageParseWorker::queueStateChanged,
        this,
        &EventHistoryService::scheduleMessagePressureNotification,
        Qt::QueuedConnection);

    m_messageAdmissionWorker = new MessageAdmissionWorker;
    m_messageAdmissionWorker->moveToThread(&m_messageAdmissionThread);
    connect(
        &m_messageAdmissionThread,
        &QThread::finished,
        m_messageAdmissionWorker,
        &QObject::deleteLater);
    connect(
        m_messageAdmissionWorker,
        &MessageAdmissionWorker::preparedAvailable,
        this,
        &EventHistoryService::applyPreparedIncomingMessages,
        Qt::QueuedConnection);
    connect(
        &m_sessionService,
        &SessionService::sessionsChanged,
        this,
        &EventHistoryService::invalidateMessageContexts);
    connect(
        &m_sessionService,
        &SessionService::currentSessionChanged,
        this,
        &EventHistoryService::invalidateMessageContexts);
    connect(
        &m_sessionService,
        &SessionService::messageCapturePolicyChanged,
        this,
        [this]() {
            invalidateMessageContexts();
            scheduleMessagePressureNotification();
        });
    connect(
        &m_preferencesController,
        &PreferencesController::maxIncomingPayloadBytesChanged,
        this,
        &EventHistoryService::invalidateMessageContexts);
    connect(
        &m_preferencesController,
        &PreferencesController::saveMessagesWhenOutputPausedChanged,
        this,
        &EventHistoryService::invalidateMessageContexts);
    m_messageAdmissionThread.start();
    QMetaObject::invokeMethod(
        m_messageAdmissionWorker,
        &MessageAdmissionWorker::start,
        Qt::BlockingQueuedConnection);
}

EventHistoryService::~EventHistoryService()
{
    shutdownIncomingMessageAdmission();
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

    if (allSessions && isMessage) {
        for (SessionState &session : m_sessionService.sessions()) {
            session.runtime.totalMessageCount = 0;
            session.runtime.viewedMessageCount = 0;
        }
    } else if (isMessage) {
        current->runtime.totalMessageCount = 0;
        current->runtime.viewedMessageCount = 0;
    }
    if (current) {
        streamRows(*current, kind).clear();
        oldestLoadedId(*current, kind) = 0;
        loadedAllHistory(*current, kind) = true;
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
        session.runtime.totalMessageCount = 0;
        session.runtime.viewedMessageCount = 0;
    }
    m_visibleStreamState = {};
    m_visibleStreamSessionId = current ? current->id : QString();
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
    const auto renderContext = isMessage
        ? subscriptionRenderContext(*session)
        : QSharedPointer<const SubscriptionRenderContext> {};
    QVector<EventRow> rows = isMessage
        ? EventRenderer::loadHistoryRows(
            m_historyStore.loadMessagesBefore(session->id, oldestId, pageSize),
            renderContext->formats,
            renderContext->colors,
            renderContext->aliases,
            m_launchTimestamp,
            false)
        : EventRenderer::loadLogRows(
            m_historyStore.loadLogsBefore(session->id, oldestId, pageSize),
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

    QVector<EventRow> merged = rows;
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
            m_frozenOldestLoadedMessageId = oldestLoadedId(*session, Stream::Message);
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

    auto &messageRows = streamRows(*session, Stream::Message);
    if (session->runtime.totalMessageCount > messageRows.size()) {
        loadedAllHistory(*session, Stream::Message) = false;
    }

    if (m_pendingVisibleMessageSessionId == session->id) {
        m_pendingVisibleMessageRows.clear();
        m_pendingVisibleMessageSessionId.clear();
        m_visibleMessageRowsFlushTimer.stop();
    }
    m_messages.setRows(messageRows);
}

void EventHistoryService::appendRenderedMessageRow(SessionState &session, const EventRow &row)
{
    appendRenderedMessageRows(session, QVector<EventRow> {row});
}

void EventHistoryService::appendRenderedMessageRows(
    SessionState &session,
    const QVector<EventRow> &rows)
{
    if (rows.isEmpty()) {
        return;
    }
    if (&session != m_sessionService.currentSession()) {
        return;
    }

    streamRows(session, Stream::Message).append(rows);
    trimVisibleRows(session, Stream::Message);

    if (!m_pendingVisibleMessageSessionId.isEmpty()
            && m_pendingVisibleMessageSessionId != session.id) {
        flushPendingVisibleMessageRows();
    }
    m_pendingVisibleMessageSessionId = session.id;
    m_pendingVisibleMessageRows.append(rows);
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
    const QVector<EventRow> rows = m_pendingVisibleMessageRows;
    m_pendingVisibleMessageRows.clear();
    m_pendingVisibleMessageSessionId.clear();

    if (!stillShowingSession) {
        return;
    }

    if (!m_messageStreamFrozen) {
        const bool pendingRowsAlreadyReflected = m_messages.rowCount()
                == streamRows(*currentSession, Stream::Message).size()
            && m_messages.lastRowEquals(rows.constLast());
        if (!pendingRowsAlreadyReflected) {
            m_messages.appendRowsAndTrimFront(rows, kMaxVisibleEventRows);
        }
    }

    if (!rows.isEmpty()) {
        emit messageRowsAppended(rows);
    }
}

void EventHistoryService::appendRenderedLogRow(SessionState &session, const EventRow &row)
{
    if (&session != m_sessionService.currentSession()) {
        return;
    }

    streamRows(session, Stream::Log).append(row);
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
    task.messageId = record.id;
    task.sequence = sequence;
    task.sessionId = record.sessionId;
    task.timestamp = record.timestamp;
    task.topic = record.topic;
    task.payloadBytes = record.payloadBytes;
    task.payloadFormat = record.payloadFormat;
    task.processorRevision = processorRevision;
    task.processorName = record.processorName;
    task.processorParameters = processorParameters;
    if (m_messageParser.enqueueTask(task)) {
        return;
    }

    ParseOutcome result;
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
        handleParseOutcome(result);
    }
}

void EventHistoryService::handleParseOutcome(const ParseOutcome &result)
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
    const ParseOutcome &result)
{
    auto applyResult = [&result](EventRow &row) {
        if (row.historyId != result.messageId) {
            return false;
        }

        const QString visibleParsedPayload = result.displayPayload.left(kVisibleParsedCharacters);
        row.parseState = messageParseStateName(result.state);
        row.parsedPayload = visibleParsedPayload;
        row.expandedPayload.clear();
        row.expandedPayloadState = QStringLiteral("idle");
        row.expandedPayloadNeeded = result.state == MessageParseState::Succeeded
            && (result.displayPayload.size() > visibleParsedPayload.size()
                || result.displayPayload.count(QLatin1Char('\n')) >= 4096);
        if (result.state == MessageParseState::Succeeded) {
            row.payload = visibleParsedPayload;
            row.payloadFormat = result.displayFormat;
            if (result.processorId.isEmpty()) {
                row.testPayload = visibleParsedPayload;
            }
        } else if (result.state == MessageParseState::Failed) {
            const QString errorLabel = result.processorId.isEmpty()
                ? QStringLiteral("Parser Error")
                : QStringLiteral("Processor Error");
            row.payload = row.testPayload.isEmpty()
                ? QStringLiteral("%1: %2").arg(errorLabel, result.displayError)
                : QStringLiteral("%1\n%2: %3").arg(
                    row.testPayload,
                    errorLabel,
                    result.displayError);
            row.payloadFormat = result.processorId.isEmpty()
                ? (result.displayFormat.isEmpty()
                    ? QStringLiteral("Parser Error")
                    : QStringLiteral("%1 Error").arg(result.displayFormat))
                : QStringLiteral("Processor Error");
        } else if (result.state == MessageParseState::SkippedOverload) {
            row.payloadFormat = QStringLiteral("Parse skipped");
        }
        return true;
    };

    std::optional<EventRow> updatedRow;
    auto &messageRows = streamRows(session, Stream::Message);
    for (auto item = messageRows.rbegin();
         item != messageRows.rend();
         ++item) {
        if (applyResult(*item)) {
            updatedRow = *item;
            break;
        }
    }
    if (!updatedRow) {
        return;
    }

    bool pendingRowUpdated = false;
    if (m_pendingVisibleMessageSessionId == session.id) {
        for (auto item = m_pendingVisibleMessageRows.rbegin();
             item != m_pendingVisibleMessageRows.rend();
             ++item) {
            if (applyResult(*item)) {
                pendingRowUpdated = true;
                break;
            }
        }
    }

    if (&session == m_sessionService.currentSession()
        && !m_messageStreamFrozen
        && !pendingRowUpdated) {
        m_messages.updateRowByHistoryId(result.messageId, *updatedRow);
    }
}

QSharedPointer<const MessageAdmissionContext> EventHistoryService::messageAdmissionContext(
    const SessionState &session)
{
    const auto cached = m_messageAdmissionContexts.constFind(session.id);
    if (cached != m_messageAdmissionContexts.cend()) {
        return cached.value();
    }

    auto context = QSharedPointer<MessageAdmissionContext>::create();
    context->capturePolicy = session.capturePolicy;
    context->subscriptionFormats = subscriptionRenderContext(session)->formats;
    context->maxPayloadBytes = m_preferencesController.maxIncomingPayloadBytes();
    context->outputPaused = session.outputPaused;
    context->saveMessagesWhenOutputPaused =
        m_preferencesController.saveMessagesWhenOutputPaused();
    context->subscriptions.reserve(session.subscriptions.size());

    for (const SubscriptionEntry &subscription : session.subscriptions) {
        MessageAdmissionSubscription snapshot;
        snapshot.topic = subscription.topic;
        snapshot.alias = subscription.alias;
        snapshot.color = subscription.color;
        snapshot.format = subscription.format;
        snapshot.paused = subscription.paused;
        snapshot.processor = subscription.processor;
        if (!snapshot.processor.processorId.isEmpty()) {
            const auto resolved = m_processorLibrary.resolve(
                snapshot.processor,
                &snapshot.processorResolutionError);
            if (resolved) {
                snapshot.processorRevision = resolved->revision;
                snapshot.processorName = resolved->processor.name;
            } else if (const auto processor = m_processorLibrary.processorById(
                           snapshot.processor.processorId)) {
                snapshot.processorName = processor->name;
            }
        }
        context->subscriptions.append(std::move(snapshot));
    }

    const QSharedPointer<const MessageAdmissionContext> immutableContext = context;
    m_messageAdmissionContexts.insert(session.id, immutableContext);
    return immutableContext;
}

QSharedPointer<const SubscriptionRenderContext> EventHistoryService::subscriptionRenderContext(
    const SessionState &session) const
{
    const auto cached = m_subscriptionRenderContexts.constFind(session.id);
    if (cached != m_subscriptionRenderContexts.cend()) {
        return cached.value();
    }

    auto context = QSharedPointer<SubscriptionRenderContext>::create();
    context->formats = session.runtime.subscriptionFormats;
    for (const SubscriptionEntry &subscription : session.subscriptions) {
        if (!subscription.color.isEmpty()) {
            context->colors.insert(subscription.topic, subscription.color);
        }
        if (!subscription.alias.isEmpty()) {
            context->aliases.insert(subscription.topic, subscription.alias);
        }
    }

    const QSharedPointer<const SubscriptionRenderContext> immutableContext = context;
    m_subscriptionRenderContexts.insert(session.id, immutableContext);
    return immutableContext;
}

void EventHistoryService::invalidateMessageContexts()
{
    m_messageAdmissionContexts.clear();
    m_subscriptionRenderContexts.clear();
}

void EventHistoryService::queueIncomingMessage(
    const QString &sessionId,
    const QString &topic,
    const QByteArray &payloadBytes,
    int qos,
    bool retain,
    const MqttPublishProperties &properties)
{
    auto *session = m_sessionService.sessionById(sessionId);
    if (!session || !m_messageAdmissionWorker) {
        return;
    }

    IncomingMessageAdmissionTask task;
    task.sessionId = sessionId;
    task.topic = topic;
    task.payloadBytes = payloadBytes;
    task.qos = qos;
    task.retain = retain;
    task.publishProperties = properties;
    task.receivedAtMs = QDateTime::currentMSecsSinceEpoch();
    task.pressureSkipsParsing = shouldSkipParsingForPressure();
    task.context = messageAdmissionContext(*session);
    if (!m_messageAdmissionWorker->enqueue(std::move(task))) {
        scheduleMessagePressureNotification();
    }
}

void EventHistoryService::applyPreparedIncomingMessages()
{
    if (!m_messageAdmissionWorker) {
        return;
    }

    QVector<PreparedIncomingMessage> prepared = m_messageAdmissionWorker->takePrepared();
    QVector<EventRow> currentVisibleRows;
    const SessionState *currentSession = m_sessionService.currentSession();
    const QString currentSessionId = currentSession ? currentSession->id : QString();
    bool totalCountChanged = false;
    bool currentSubscriptionActivityChanged = false;
    QHash<QString, QVector<TopicObservation>> topicObservations;
    for (PreparedIncomingMessage &message : prepared) {
        auto *session = m_sessionService.sessionById(message.sessionId);
        if (!session) {
            continue;
        }

        session->runtime.recentReceivedTraffic.add(
            message.receivedAtMs,
            message.payloadBytes);
        if (!message.captured) {
            recordCaptureFiltered();
            continue;
        }

        for (SubscriptionEntry &subscription : session->subscriptions) {
            if (message.activeSubscriptionTopics.contains(subscription.topic)) {
                subscription.recentMessages.add(message.receivedAtMs);
                currentSubscriptionActivityChanged = currentSubscriptionActivityChanged
                    || session == m_sessionService.currentSession();
            }
        }

        if (!message.reportMessage.isEmpty()
            && !m_reportedPayloadStorageStates.contains(message.reportKey)) {
            m_reportedPayloadStorageStates.insert(message.reportKey);
            appendEvent(
                *session,
                QStringLiteral("Payload"),
                message.reportMessage);
        }

        const qint64 sequence = m_nextMessageSequence.value(message.sessionId) + 1;
        const qint64 historyId = m_historyWriter.enqueueMessage(message.record);
        if (historyId <= 0) {
            continue;
        }

        message.record.id = historyId;
        message.sequence = sequence;
        m_nextMessageSequence.insert(message.sessionId, sequence);
        topicObservations[message.sessionId].append({
            .topic = message.record.topic,
            .historyId = historyId,
            .observedAtMs = message.receivedAtMs,
            .payloadPreview = message.record.payloadPreview,
        });
        if (message.parsingSkippedForPressure) {
            recordPressureSkippedParse();
        }
        ++session->runtime.totalMessageCount;
        if (session == m_sessionService.currentSession()) {
            session->runtime.viewedMessageCount = session->runtime.totalMessageCount;
        }
        totalCountChanged = true;
        m_lastMessageStorageError.clear();

        if (session->id == currentSessionId && !session->outputPaused) {
            message.renderedRow.historyId = historyId;
            currentVisibleRows.append(message.renderedRow);
        }

    }

    if (!currentVisibleRows.isEmpty()) {
        if (auto *session = m_sessionService.sessionById(currentSessionId)) {
            appendRenderedMessageRows(*session, currentVisibleRows);
        }
    }

    for (auto it = topicObservations.cbegin(); it != topicObservations.cend(); ++it) {
        emit incomingTopicsObserved(it.key(), it.value());
    }

    for (const PreparedIncomingMessage &message : std::as_const(prepared)) {
        if (message.parsingRequired && message.record.id > 0) {
            enqueueMessageParsing(
                message.record,
                message.sequence,
                message.processorRevision,
                message.processorParameters);
        }
    }

    scheduleMessageActivityNotification(
        totalCountChanged,
        currentSubscriptionActivityChanged);
    scheduleMessagePressureNotification();
}

void EventHistoryService::appendPublishedMessage(
    const QString &sessionId,
    const QString &topic,
    const QByteArray &payloadBytes,
    int format,
    int qos,
    bool retain,
    const MqttPublishProperties &properties)
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
    const MessagePayload::Plan payloadPlan = MessagePayload::planStorage(
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
    record.publishProperties = properties;
    bool parsingRequired = MessagePayload::requiresBackgroundParse(
        PayloadCodec::formatFromInt(format));
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
        const auto renderContext = subscriptionRenderContext(*session);
        appendRenderedMessageRow(
            *session,
            EventRenderer::renderMessageRow(
                record,
                renderContext->formats,
                renderContext->colors,
                renderContext->aliases));
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

bool EventHistoryService::requestExpandedMessage(qint64 messageId)
{
    if (!m_messages.beginExpandedPayloadLoad(messageId)) {
        return false;
    }
    const bool invoked = QMetaObject::invokeMethod(
        &m_historyWriter,
        "loadExpandedMessage",
        Qt::QueuedConnection,
        Q_ARG(qint64, messageId));
    if (!invoked) {
        m_messages.finishExpandedPayloadLoad(
            messageId,
            QString(),
            QStringLiteral("unavailable"));
    }
    return invoked;
}

QVariantMap EventHistoryService::messageDetails(qint64 messageId) const
{
    if (messageId <= 0) {
        return {};
    }

    std::optional<MessageRecord> stored;
    if (const auto pending = m_historyWriter.pendingMessage(messageId)) {
        stored = *pending;
    } else {
        stored = m_historyStore.loadMessage(messageId);
        if (stored) {
            if (const auto pendingResult = m_historyWriter.pendingParseResult(messageId)) {
                applyParseOutcome(*stored, *pendingResult);
            }
        }
    }
    if (!stored) {
        return {};
    }

    QSharedPointer<const SubscriptionRenderContext> renderContext;
    if (const auto *session = m_sessionService.currentSession()) {
        renderContext = subscriptionRenderContext(*session);
    }

    QVariantMap details = eventRowToVariantMap(EventRenderer::renderMessageRow(
        *stored,
        renderContext ? renderContext->formats : QHash<QString, int> {},
        renderContext ? renderContext->colors : QHash<QString, QString> {},
        renderContext ? renderContext->aliases : QHash<QString, QString> {}));
    details.insert(
        QStringLiteral("payloadFormat"),
        PayloadCodec::formatName(PayloadCodec::formatFromInt(
            stored->payloadFormat)));
    details.insert(QStringLiteral("parsedPayload"), stored->displayPayload);
    details.insert(QStringLiteral("parseError"), stored->displayError);
    details.insert(QStringLiteral("parseState"), stored->displayState);
    details.insert(QStringLiteral("processorId"), stored->processorId);
    details.insert(QStringLiteral("processorRevisionId"), stored->processorRevisionId);
    details.insert(QStringLiteral("processorName"), stored->processorName);
    details.insert(QStringLiteral("processorExecutionState"), stored->processorExecutionState);
    details.insert(
        QStringLiteral("processorExecutionErrorCode"),
        stored->processorExecutionErrorCode);
    details.insert(QStringLiteral("processorExecutionError"), stored->processorExecutionError);
    const QVariantMap mqttProperties = mqttPublishPropertiesToVariantMap(
        stored->publishProperties);
    details.insert(QStringLiteral("mqttProperties"), mqttProperties);
    QStringList propertyLines;
    for (auto it = mqttProperties.cbegin(); it != mqttProperties.cend(); ++it) {
        if (it.key() == QStringLiteral("userProperties")) {
            for (const QVariant &propertyValue : it.value().toList()) {
                const QVariantMap property = propertyValue.toMap();
                propertyLines.append(QStringLiteral("%1 = %2").arg(
                    property.value(QStringLiteral("name")).toString(),
                    property.value(QStringLiteral("value")).toString()));
            }
        } else if (it.key() == QStringLiteral("subscriptionIdentifiers")) {
            QStringList identifiers;
            for (const QVariant &identifier : it.value().toList()) {
                identifiers.append(identifier.toString());
            }
            propertyLines.append(QStringLiteral("subscriptionIdentifiers: %1").arg(
                identifiers.join(QStringLiteral(", "))));
        } else {
            propertyLines.append(QStringLiteral("%1: %2").arg(it.key(), it.value().toString()));
        }
    }
    details.insert(QStringLiteral("mqttPropertiesText"), propertyLines.join(QLatin1Char('\n')));
    const bool fullPayloadAvailable = stored->payloadState != QStringLiteral("skipped")
        && (stored->payloadSize == 0 || !stored->payloadBytes.isEmpty());

    QString fullPayload;
    if (fullPayloadAvailable) {
        QString decodeError;
        fullPayload = PayloadCodec::decodeForDisplay(
            PayloadCodec::formatFromInt(stored->payloadFormat),
            stored->payloadBytes,
            decodeError);
        if (!decodeError.isEmpty()) {
            fullPayload = stored->payloadPreview;
        }
    } else {
        fullPayload = stored->payloadPreview;
    }

    details.insert(QStringLiteral("fullPayloadAvailable"), fullPayloadAvailable);
    details.insert(QStringLiteral("fullPayload"), fullPayload);
    details.insert(QStringLiteral("payloadPreview"), stored->payloadPreview);
    details.insert(QStringLiteral("payloadHash"), stored->payloadHash);
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
    const QVector<ParseOutcome> pendingParseResults =
        m_historyWriter.pendingParseResults(session->id);
    QVector<MessageRecord> messageRows = m_historyStore.loadMessages(session->id, pageSize);
    const bool loadedAllPersistedMessageHistory = messageRows.size() < pageSize;
    QMap<qint64, MessageRecord> messageRowsById;
    for (const MessageRecord &row : std::as_const(messageRows)) {
        messageRowsById.insert(row.id, row);
    }
    for (const MessageRecord &message : pendingMessages) {
        messageRowsById.insert(message.id, message);
    }
    for (const ParseOutcome &parseResult : pendingParseResults) {
        auto row = messageRowsById.find(parseResult.messageId);
        if (row != messageRowsById.end()) {
            applyParseOutcome(row.value(), parseResult);
        }
    }
    while (messageRowsById.size() > pageSize) {
        messageRowsById.erase(messageRowsById.begin());
    }
    messageRows.clear();
    for (const MessageRecord &row : std::as_const(messageRowsById)) {
        messageRows.append(row);
    }
    const auto renderContext = subscriptionRenderContext(*session);
    auto &visibleMessageRows = streamRows(*session, Stream::Message);
    visibleMessageRows = EventRenderer::loadHistoryRows(
        messageRows,
        renderContext->formats,
        renderContext->colors,
        renderContext->aliases,
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
    oldestLoadedId(*session, Stream::Message) = EventRenderer::firstHistoryId(visibleMessageRows);
    loadedAllHistory(*session, Stream::Message) = loadedAllPersistedMessageHistory;
    m_messages.setRows(visibleMessageRows);

    const QVariantList logRows = m_historyStore.loadLogs(session->id, pageSize);
    auto &visibleLogRows = streamRows(*session, Stream::Log);
    visibleLogRows = EventRenderer::loadLogRows(
        logRows,
        m_launchTimestamp,
        true);
    oldestLoadedId(*session, Stream::Log) = EventRenderer::firstHistoryId(visibleLogRows);
    loadedAllHistory(*session, Stream::Log) = logRows.size() < pageSize;
    m_logs.setRows(visibleLogRows);
}

bool EventHistoryService::flushPendingMessageHistory(int timeoutMs)
{
    const bool admissionDrained = flushPendingIncomingMessages(timeoutMs);
    flushPendingMessageActivityNotifications();
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

    if (!admissionDrained) {
        m_lastMessageStorageError = QStringLiteral("Timed out while preparing incoming messages.");
        return false;
    }

    m_lastMessageStorageError.clear();
    return true;
}

bool EventHistoryService::flushPendingIncomingMessages(int timeoutMs)
{
    if (!m_messageAdmissionWorker) {
        return true;
    }
    const bool drained = m_messageAdmissionWorker->drain(timeoutMs);
    applyPreparedIncomingMessages();
    return drained;
}

void EventHistoryService::stopAcceptingIncomingMessages()
{
    if (m_messageAdmissionWorker) {
        m_messageAdmissionWorker->stopAccepting();
    }
}

void EventHistoryService::shutdownIncomingMessageAdmission()
{
    if (!m_messageAdmissionWorker) {
        return;
    }

    stopAcceptingIncomingMessages();
    flushPendingIncomingMessages();
    flushPendingMessageActivityNotifications();
    QMetaObject::invokeMethod(
        m_messageAdmissionWorker,
        &MessageAdmissionWorker::shutdown,
        Qt::BlockingQueuedConnection);
    m_messageAdmissionThread.quit();
    m_messageAdmissionThread.wait();
    m_messageAdmissionWorker = nullptr;
}

void EventHistoryService::stopAcceptingMessageParsing()
{
    m_messageParser.stopAccepting();
}

int EventHistoryService::messageWriterBacklog() const
{
    return m_historyWriter.pendingMessageCount()
        + (m_messageAdmissionWorker
               ? m_messageAdmissionWorker->pendingMessageCount()
               : 0);
}

qint64 EventHistoryService::messageWriterBacklogBytes() const
{
    return m_historyWriter.pendingBytes()
        + (m_messageAdmissionWorker
               ? m_messageAdmissionWorker->pendingBytes()
               : 0);
}

qint64 EventHistoryService::droppedMessageCount() const
{
    return m_historyWriter.droppedMessageCount()
        + (m_messageAdmissionWorker
               ? m_messageAdmissionWorker->droppedMessageCount()
               : 0);
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
    const auto admissionState = m_messageAdmissionWorker
        ? m_messageAdmissionWorker->pressureState()
        : MessageAdmissionWorker::PressureState::Normal;
    if (writerState == HistoryWriterWorker::PressureState::Dropping
        || admissionState == MessageAdmissionWorker::PressureState::Dropping) {
        return QStringLiteral("dropping");
    }
    if (writerState == HistoryWriterWorker::PressureState::Degraded
        || parserState == MessageParseWorker::PressureState::Dropping) {
        return QStringLiteral("degraded");
    }
    if (writerState == HistoryWriterWorker::PressureState::Elevated
        || parserState == MessageParseWorker::PressureState::Elevated
        || admissionState == MessageAdmissionWorker::PressureState::Elevated) {
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
    if (m_messageAdmissionWorker) {
        switch (m_messageAdmissionWorker->pressureState()) {
        case MessageAdmissionWorker::PressureState::Dropping:
            return QStringLiteral("dropping");
        case MessageAdmissionWorker::PressureState::Elevated:
            return QStringLiteral("elevated");
        case MessageAdmissionWorker::PressureState::Normal:
            break;
        }
    }
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

void EventHistoryService::scheduleMessageActivityNotification(
    bool totalCountChanged,
    bool subscriptionActivityDirty)
{
    m_totalMessageCountNotificationPending =
        m_totalMessageCountNotificationPending || totalCountChanged;
    m_subscriptionActivityNotificationPending =
        m_subscriptionActivityNotificationPending || subscriptionActivityDirty;
    if ((m_totalMessageCountNotificationPending
            || m_subscriptionActivityNotificationPending)
        && !m_messageActivityNotificationTimer.isActive()) {
        m_messageActivityNotificationTimer.start();
    }
}

void EventHistoryService::flushPendingMessageActivityNotifications()
{
    m_messageActivityNotificationTimer.stop();
    const bool totalCountChanged = m_totalMessageCountNotificationPending;
    const bool subscriptionActivityDirty = m_subscriptionActivityNotificationPending;
    m_totalMessageCountNotificationPending = false;
    m_subscriptionActivityNotificationPending = false;
    if (subscriptionActivityDirty) {
        emit subscriptionActivityChanged();
    }
    if (totalCountChanged) {
        emit totalMessageCountChanged();
    }
}

bool EventHistoryService::shouldCaptureMessage(
    const SessionState &session,
    MessageDirection direction,
    const QString &topic) const
{
    return session.capturePolicy.accepts(direction, topic);
}

bool EventHistoryService::shouldSkipParsingForPressure() const
{
    return m_historyWriter.pressureState() != HistoryWriterWorker::PressureState::Normal
        || m_messageParser.pressureState() != MessageParseWorker::PressureState::Normal
        || (m_messageAdmissionWorker
            && m_messageAdmissionWorker->pressureState()
                != MessageAdmissionWorker::PressureState::Normal);
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
