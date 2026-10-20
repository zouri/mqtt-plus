#include "viewmodels/workbenchviewmodel.h"

#include "usecases/eventhistoryservice.h"
#include "usecases/mqttsessionservice.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"
#include "domain/publishstatus.h"
#include "domain/sessionconfig.h"
#include "services/apputils.h"
#include "domain/messagecapturepolicy.h"
#include "services/payload/payloadcodec.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QRegularExpression>

#include <algorithm>
#include <utility>

using namespace AppUtils;

namespace {
constexpr int kDisplayTotalMessageCountIntervalMs = 50;
constexpr int kPausedDisplayTotalMessageCountIntervalMs = 250;
constexpr int kTrafficRateIntervalMs = 1000;

QStringList topicFiltersFromText(const QString &text)
{
    return text.split(QRegularExpression(QStringLiteral("[,\\r\\n]+")), Qt::SkipEmptyParts);
}

} // namespace

WorkbenchViewModel::WorkbenchViewModel(
    SessionService &sessionService,
    MqttSessionService &mqttService,
    DraftLibraryService &draftService,
    DraftLibraryModel &draftsModel,
    SubscriptionService &subscriptionService,
    EventHistoryService &eventHistoryService,
    SessionListModel &sessionsModel,
    SubscriptionFilterModel &filteredSubscriptionsModel,
    TopicTreeModel &topicTreeModel,
    EventStreamModel &messagesModel,
    MessageFilterModel &filteredMessagesModel,
    ProcessorLibraryModel &processorsModel,
    QObject *parent)
    : QObject(parent)
    , m_sessionService(sessionService)
    , m_mqttService(mqttService)
    , m_subscriptionService(subscriptionService)
    , m_eventHistoryService(eventHistoryService)
    , m_sessionsModel(sessionsModel)
    , m_filteredSubscriptionsModel(filteredSubscriptionsModel)
    , m_topicTreeModel(topicTreeModel)
    , m_messagesModel(messagesModel)
    , m_filteredMessagesModel(filteredMessagesModel)
    , m_processorsModel(processorsModel)
    , m_publisher(sessionService, mqttService, draftService, draftsModel, this)
{
    m_displayTotalMessageCountTimer.setInterval(kDisplayTotalMessageCountIntervalMs);
    m_displayTotalMessageCountTimer.setSingleShot(true);
    connect(
        &m_displayTotalMessageCountTimer,
        &QTimer::timeout,
        this,
        &WorkbenchViewModel::syncDisplayTotalMessageCount);

    connect(&m_filteredMessagesModel, &QAbstractItemModel::rowsInserted, this, [this]()
    {
        const SessionState *session = m_sessionService.currentSession();
        if (!session || session->outputPaused)
        {
            return;
        }
        // Share the list's update frame; retain the timer when no visible rows arrive.
        m_displayTotalMessageCountTimer.stop();
        syncDisplayTotalMessageCount();
    });

    m_trafficRateTimer.setInterval(kTrafficRateIntervalMs);
    m_trafficRateTimer.setTimerType(Qt::CoarseTimer);
    connect(
        &m_trafficRateTimer,
        &QTimer::timeout,
        this,
        &WorkbenchViewModel::refreshTrafficRates);
    m_trafficRateTimer.start();

    connect(&m_sessionService,
            &SessionService::currentSessionIndexChanged,
            this,
            &WorkbenchViewModel::currentSessionIndexChanged);
    connect(&m_sessionService,
            &SessionService::currentSessionChanged,
            this,
            &WorkbenchViewModel::currentSessionChanged);
    connect(&m_sessionService,
            &SessionService::currentSessionChanged,
            this,
            &WorkbenchViewModel::sessionStatusChanged);
    connect(&m_sessionService,
            &SessionService::currentSessionChanged,
            this,
            &WorkbenchViewModel::publishStatusChanged);
    connect(&m_sessionService,
            &SessionService::currentSessionChanged,
            this,
            &WorkbenchViewModel::totalMessageCountChanged);
    connect(&m_sessionService,
            &SessionService::currentSessionChanged,
            this,
            [this]() {
                m_displayTotalMessageCountTimer.stop();
                syncDisplayTotalMessageCount();
                refreshTrafficRates();
            });
    connect(&m_sessionService,
            &SessionService::currentSessionChanged,
            this,
            &WorkbenchViewModel::messageStreamChanged);
    connect(&m_sessionService,
            &SessionService::currentSessionChanged,
            this,
            &WorkbenchViewModel::messageCapturePolicyChanged);
    connect(&m_sessionService,
            &SessionService::messageCapturePolicyChanged,
            this,
            [this](const QString &sessionId) {
                const SessionState *session = m_sessionService.currentSession();
                if (session && session->id == sessionId) {
                    emit messageCapturePolicyChanged();
                }
            });
    connect(&m_mqttService,
            &MqttSessionService::sessionStateChanged,
            this,
            &WorkbenchViewModel::sessionStatusChanged);
    connect(&m_mqttService,
            &MqttSessionService::sessionStateChanged,
            this,
            &WorkbenchViewModel::publishStatusChanged);
    connect(&m_eventHistoryService,
            &EventHistoryService::messageStreamChanged,
            this,
            &WorkbenchViewModel::messageStreamChanged);
    connect(&m_eventHistoryService,
            &EventHistoryService::messageStreamChanged,
            this,
            &WorkbenchViewModel::totalMessageCountChanged);
    connect(&m_eventHistoryService,
            &EventHistoryService::messageStreamChanged,
            this,
            [this]() {
                m_displayTotalMessageCountTimer.stop();
                syncDisplayTotalMessageCount();
            });
    connect(&m_eventHistoryService,
            &EventHistoryService::totalMessageCountChanged,
            this,
            &WorkbenchViewModel::totalMessageCountChanged);
    connect(&m_eventHistoryService,
            &EventHistoryService::totalMessageCountChanged,
            this,
            &WorkbenchViewModel::scheduleDisplayTotalMessageCountUpdate);
    connect(&m_eventHistoryService,
            &EventHistoryService::messageWriterStateChanged,
            this,
            &WorkbenchViewModel::refreshMessagePressure);
    connect(&m_eventHistoryService,
            &EventHistoryService::messageRowsAppended,
            this,
            [this](const QVector<EventRow> &rows) {
                const int visibleMessageCount = m_filteredMessagesModel.matchingMessageCount(rows);
                if (visibleMessageCount > 0) {
                    emit messageStreamRowsAppended(visibleMessageCount);
                }
            });
    connect(&m_eventHistoryService,
            &EventHistoryService::messageParseResultChanged,
            this,
            [this](qint64 historyId) {
                emit messageDetailsChanged(QString::number(historyId));
            });
    connect(&m_processorsModel,
            &QAbstractItemModel::modelReset,
            this,
            &WorkbenchViewModel::refreshSubscriptionEditorProcessorOptions);
    connect(&m_processorsModel,
            &QAbstractItemModel::dataChanged,
            this,
            &WorkbenchViewModel::refreshSubscriptionEditorProcessorOptions);
    connect(&m_subscriptionService,
            &SubscriptionService::subscriptionsChanged,
            this,
            &WorkbenchViewModel::subscriptionsStateChanged);
    m_displayTotalMessageCount = totalMessageCount();
    refreshTrafficRates();
    refreshMessagePressure();
    refreshSubscriptionEditorProcessorOptions();
}

SessionListModel *WorkbenchViewModel::sessions() const { return &m_sessionsModel; }
SubscriptionFilterModel *WorkbenchViewModel::filteredSubscriptions() const { return &m_filteredSubscriptionsModel; }
TopicTreeModel *WorkbenchViewModel::topicTree() const { return &m_topicTreeModel; }
EventStreamModel *WorkbenchViewModel::messages() const { return &m_messagesModel; }
MessageFilterModel *WorkbenchViewModel::filteredMessages() const { return &m_filteredMessagesModel; }
PublishComposerViewModel *WorkbenchViewModel::publisher() { return &m_publisher; }
SessionEditorViewModel *WorkbenchViewModel::sessionEditor() { return &m_sessionEditor; }
SubscriptionEditorViewModel *WorkbenchViewModel::subscriptionEditor() { return &m_subscriptionEditor; }
int WorkbenchViewModel::currentSessionIndex() const
{
    return m_sessionService.currentIndex();
}

QVariantMap WorkbenchViewModel::currentSession() const
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session) {
        return {};
    }

    QVariantMap row;
    const auto *client = session->runtime.client;
    row.insert(QStringLiteral("id"), session->id);
    row.insert(QStringLiteral("name"), session->name);
    row.insert(QStringLiteral("host"), client ? client->hostname() : QString());
    row.insert(QStringLiteral("port"), client ? client->port() : SessionConfig::kDefaultPort);
    row.insert(QStringLiteral("transport"), session->transport);
    row.insert(QStringLiteral("transportLabel"), SessionConfig::transportLabel(session->transport));
    row.insert(QStringLiteral("protocolVersion"), session->protocolVersion);
    row.insert(QStringLiteral("protocolVersionName"), protocolVersionLabel(session->protocolVersion));
    row.insert(QStringLiteral("clientId"), client ? client->clientId() : QString());
    row.insert(QStringLiteral("username"), client ? client->username() : QString());
    row.insert(QStringLiteral("cleanSession"), client ? client->cleanSession() : true);
    row.insert(QStringLiteral("keepAliveSeconds"), client ? client->keepAlive() : SessionConfig::kDefaultKeepAlive);
    row.insert(QStringLiteral("outputPaused"), session->outputPaused);
    row.insert(QStringLiteral("subscriptionCount"), session->subscriptions.size());
    return row;
}

QVariantMap WorkbenchViewModel::sessionStatus() const
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session) {
        return {};
    }

    const auto *client = session->runtime.client;
    const QString state = sessionStateName(*session, client);
    QString summary;
    if (state == QStringLiteral("connected")) {
        summary = QCoreApplication::translate("WorkbenchViewModel", "%1 • %2:%3 • %4")
                      .arg(protocolVersionLabel(session->protocolVersion))
                      .arg(client ? client->hostname() : QString())
                      .arg(client ? client->port() : SessionConfig::kDefaultPort)
                      .arg(SessionConfig::transportLabel(session->transport));
        if (session->runtime.sessionRestored) {
            summary.append(QCoreApplication::translate("WorkbenchViewModel", " • session restored"));
        }
    } else if (state == QStringLiteral("connecting")) {
        summary = QCoreApplication::translate("WorkbenchViewModel", "Connecting to %1:%2 over %3")
                      .arg(client ? client->hostname() : QString())
                      .arg(client ? client->port() : SessionConfig::kDefaultPort)
                      .arg(SessionConfig::transportLabel(session->transport));
    } else if (state == QStringLiteral("disconnecting")) {
        summary = QCoreApplication::translate("WorkbenchViewModel", "Disconnecting from broker");
    } else if (!session->runtime.lastError.isEmpty()) {
        summary = session->runtime.lastError;
    } else {
        summary = QCoreApplication::translate("WorkbenchViewModel", "Disconnected");
    }

    QVariantMap row;
    row.insert(QStringLiteral("state"), state);
    row.insert(QStringLiteral("connected"), state == QStringLiteral("connected"));
    row.insert(QStringLiteral("summary"), summary);
    row.insert(QStringLiteral("lastError"), session->runtime.lastError);
    row.insert(QStringLiteral("hasError"), !session->runtime.lastError.isEmpty());
    row.insert(QStringLiteral("brokerInfo"), session->runtime.brokerInfo);
    row.insert(QStringLiteral("sessionRestored"), session->runtime.sessionRestored);
    row.insert(QStringLiteral("transportLabel"), SessionConfig::transportLabel(session->transport));
    row.insert(QStringLiteral("protocolVersionName"), protocolVersionLabel(session->protocolVersion));
    row.insert(QStringLiteral("connectedAtMs"), session->runtime.connectedAtMs);
    row.insert(QStringLiteral("connectionStartedAtMs"), session->runtime.connectionStartedAtMs);
    row.insert(QStringLiteral("connectTimeoutSeconds"), session->connectTimeoutSeconds);
    return row;
}

QVariantMap WorkbenchViewModel::publishStatus() const
{
    const SessionState *session = m_sessionService.currentSession();
    QVariantMap status = session ? session->runtime.publishStatus.toVariantMap() : PublishStatus {}.toVariantMap();
    status.insert(
        QStringLiteral("updatedAt"),
        displayTimestamp(status.value(QStringLiteral("updatedAt")).toString()));
    return status;
}

QStringList WorkbenchViewModel::payloadFormats() const { return PayloadCodec::formatNames(); }
QString WorkbenchViewModel::pendingSubscriptionDeleteTopic() const { return m_pendingSubscriptionDeleteTopic; }
QString WorkbenchViewModel::pendingSubscriptionDeleteDisplayName() const { return m_pendingSubscriptionDeleteDisplayName; }
bool WorkbenchViewModel::allSubscriptionsPaused() const
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session || session->subscriptions.isEmpty()) {
        return false;
    }
    return std::all_of(
        session->subscriptions.cbegin(),
        session->subscriptions.cend(),
        [](const SubscriptionEntry &entry) { return entry.paused; });
}

QVariantMap WorkbenchViewModel::messagePressure() const
{
    const MessagePressure &pressure = m_messagePressure;
    return {
        {QStringLiteral("state"), pressure.state},
        {QStringLiteral("captureMode"), pressure.captureMode},
        {QStringLiteral("writerState"), pressure.writerState},
        {QStringLiteral("parserState"), pressure.parserState},
        {QStringLiteral("backlog"), pressure.writerBacklog},
        {QStringLiteral("backlogBytes"), pressure.writerBacklogBytes},
        {QStringLiteral("dropped"), pressure.droppedCount},
        {QStringLiteral("parseBacklog"), pressure.parserBacklog},
        {QStringLiteral("parseBacklogBytes"), pressure.parserBacklogBytes},
        {QStringLiteral("parseDropped"), pressure.droppedParseTaskCount},
        {QStringLiteral("parseResultDropped"), pressure.droppedParseResultCount},
        {QStringLiteral("captureFiltered"), pressure.captureFilteredCount},
        {QStringLiteral("parseSkippedPressure"), pressure.skippedParseCount},
        {QStringLiteral("storageDegraded"), pressure.storageDegraded},
        {QStringLiteral("lastError"), pressure.storageError},
    };
}

QString WorkbenchViewModel::messagePressureState() const { return m_messagePressure.state; }
QString WorkbenchViewModel::messagePressureCaptureMode() const { return m_messagePressure.captureMode; }
bool WorkbenchViewModel::messagePressureStorageDegraded() const { return m_messagePressure.storageDegraded; }
int WorkbenchViewModel::messageWriterBacklog() const { return m_messagePressure.writerBacklog; }
qint64 WorkbenchViewModel::messageWriterBacklogBytes() const { return m_messagePressure.writerBacklogBytes; }
int WorkbenchViewModel::messageParserBacklog() const { return m_messagePressure.parserBacklog; }
qint64 WorkbenchViewModel::messageParserBacklogBytes() const { return m_messagePressure.parserBacklogBytes; }
qint64 WorkbenchViewModel::messageDroppedCount() const { return m_messagePressure.droppedCount; }
qint64 WorkbenchViewModel::messageDroppedParseTaskCount() const { return m_messagePressure.droppedParseTaskCount; }
qint64 WorkbenchViewModel::messageDroppedParseResultCount() const { return m_messagePressure.droppedParseResultCount; }
qint64 WorkbenchViewModel::messageCaptureFilteredCount() const { return m_messagePressure.captureFilteredCount; }
qint64 WorkbenchViewModel::messagePressureSkippedParseCount() const { return m_messagePressure.skippedParseCount; }
QString WorkbenchViewModel::messageStorageError() const { return m_messagePressure.storageError; }

void WorkbenchViewModel::refreshMessagePressure()
{
    MessagePressure next;
    next.state = m_eventHistoryService.messagePressureState();
    next.captureMode = m_eventHistoryService.messageCaptureMode();
    next.writerState = m_eventHistoryService.messageWriterPressureState();
    next.parserState = m_eventHistoryService.messageParserPressureState();
    next.writerBacklog = m_eventHistoryService.messageWriterBacklog();
    next.writerBacklogBytes = m_eventHistoryService.messageWriterBacklogBytes();
    next.droppedCount = m_eventHistoryService.droppedMessageCount();
    next.parserBacklog = m_eventHistoryService.messageParserBacklog();
    next.parserBacklogBytes = m_eventHistoryService.messageParserBacklogBytes();
    next.droppedParseTaskCount = m_eventHistoryService.droppedParseTaskCount();
    next.droppedParseResultCount = m_eventHistoryService.droppedParseResultCount();
    next.captureFilteredCount = m_eventHistoryService.captureFilteredMessageCount();
    next.skippedParseCount = m_eventHistoryService.pressureSkippedParseCount();
    next.storageDegraded = m_eventHistoryService.messageStorageDegraded();
    next.storageError = m_eventHistoryService.messageStorageError();

    if (next == m_messagePressure) {
        return;
    }
    m_messagePressure = next;
    emit messagePressureChanged();
}

QVariantMap WorkbenchViewModel::messageCapturePolicy() const
{
    const SessionState *session = m_sessionService.currentSession();
    const MessageCapturePolicy policy = session
        ? session->capturePolicy
        : MessageCapturePolicy {};
    return {
        {QStringLiteral("captureIncoming"), policy.captureIncoming},
        {QStringLiteral("captureOutgoing"), policy.captureOutgoing},
        {QStringLiteral("includeTopicFilters"), policy.includeTopicFilters},
        {QStringLiteral("excludeTopicFilters"), policy.excludeTopicFilters},
    };
}

qint64 WorkbenchViewModel::totalMessageCount() const
{
    const SessionState *session = m_sessionService.currentSession();
    return session ? session->runtime.totalMessageCount : 0;
}

qint64 WorkbenchViewModel::displayTotalMessageCount() const
{
    return m_displayTotalMessageCount;
}

qint64 WorkbenchViewModel::incomingByteRate() const
{
    return m_incomingByteRate;
}

qint64 WorkbenchViewModel::outgoingByteRate() const
{
    return m_outgoingByteRate;
}

void WorkbenchViewModel::scheduleDisplayTotalMessageCountUpdate()
{
    if (!m_displayTotalMessageCountTimer.isActive()) {
        const SessionState *session = m_sessionService.currentSession();
        m_displayTotalMessageCountTimer.setInterval(session && session->outputPaused
            ? kPausedDisplayTotalMessageCountIntervalMs
            : kDisplayTotalMessageCountIntervalMs);
        m_displayTotalMessageCountTimer.start();
    }
}

void WorkbenchViewModel::syncDisplayTotalMessageCount()
{
    const qint64 count = totalMessageCount();
    if (m_displayTotalMessageCount == count) {
        return;
    }

    m_displayTotalMessageCount = count;
    emit displayTotalMessageCountChanged();
}

void WorkbenchViewModel::refreshTrafficRates()
{
    const SessionState *session = m_sessionService.currentSession();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 incomingRate = session
        ? session->runtime.recentReceivedTraffic.byteCount(nowMs)
        : 0;
    const qint64 outgoingRate = session
        ? session->runtime.recentPublishedTraffic.byteCount(nowMs)
        : 0;
    if (m_incomingByteRate == incomingRate && m_outgoingByteRate == outgoingRate) {
        return;
    }

    m_incomingByteRate = incomingRate;
    m_outgoingByteRate = outgoingRate;
    emit trafficRatesChanged();
}

qreal WorkbenchViewModel::currentIncomingMessageRate() const
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session) {
        return 0;
    }

    return session->runtime.recentReceivedTraffic.eventCount(
        QDateTime::currentMSecsSinceEpoch());
}

qreal WorkbenchViewModel::currentOutgoingMessageRate() const
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session) {
        return 0;
    }
    return session->runtime.recentPublishedTraffic.eventCount(
        QDateTime::currentMSecsSinceEpoch());
}

qint64 WorkbenchViewModel::currentIncomingByteRate() const
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session) {
        return 0;
    }

    return session->runtime.recentReceivedTraffic.byteCount(
        QDateTime::currentMSecsSinceEpoch());
}

qint64 WorkbenchViewModel::currentOutgoingByteRate() const
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session) {
        return 0;
    }

    return session->runtime.recentPublishedTraffic.byteCount(
        QDateTime::currentMSecsSinceEpoch());
}

void WorkbenchViewModel::setCurrentSessionIndex(int index)
{
    m_sessionService.setCurrentSessionIndex(index);
}

void WorkbenchViewModel::openSessionEditorForCreate()
{
    m_sessionEditor.openForCreate(m_sessionService.defaultSessionConfig());
}

int WorkbenchViewModel::sessionIndexById(const QString &sessionId) const
{
    const auto &sessions = std::as_const(m_sessionService).sessions();
    if (!sessionId.isEmpty()) {
        for (qsizetype index = 0; index < sessions.size(); ++index) {
            if (sessions.at(index).id == sessionId) {
                return static_cast<int>(index);
            }
        }
    }
    return -1;
}

bool WorkbenchViewModel::openSessionEditorForEdit(const QString &sessionId)
{
    const int index = sessionIndexById(sessionId);
    if (index < 0) {
        return false;
    }
    m_sessionEditor.openForEdit(sessionId, m_sessionService.sessionConfigAt(index));
    return true;
}

bool WorkbenchViewModel::submitSessionEditor()
{
    if (!m_sessionEditor.validate()) {
        return false;
    }
    const SessionConnectionConfig config = m_sessionEditor.collectedConfig();
    if (!m_sessionEditor.editMode()) {
        return m_sessionService.addSessionWithConfig(config);
    }

    const int index = sessionIndexById(m_sessionEditor.targetSessionId());
    return index >= 0 && m_sessionService.updateSessionConfigAt(index, config);
}

void WorkbenchViewModel::requestSessionDuplicate(const QString &sessionId)
{
    const int index = sessionIndexById(sessionId);
    if (index < 0) {
        return;
    }

    m_sessionService.duplicateSessionAt(index);
}

void WorkbenchViewModel::requestSessionDelete(const QString &sessionId)
{
    const int index = sessionIndexById(sessionId);
    if (index < 0 || m_sessionService.sessions().size() <= 1) {
        return;
    }

    m_sessionService.removeSessionAt(index);
}

bool WorkbenchViewModel::moveSession(const QString &sessionId, const QString &anchorId, bool after)
{
    return m_sessionService.moveSession(sessionId, anchorId, after);
}

bool WorkbenchViewModel::moveSubscription(
    const QString &sessionId, const QString &topic, const QString &anchorTopic, bool after)
{
    const SessionState *session = m_sessionService.currentSession();
    return session && !sessionId.isEmpty() && session->id == sessionId
        && m_subscriptionService.moveCurrentSubscription(topic, anchorTopic, after);
}

void WorkbenchViewModel::toggleCurrentSessionConnection()
{
    const QString state = sessionStatus().value(QStringLiteral("state")).toString();
    if (state == QStringLiteral("connected")
        || state == QStringLiteral("connecting")
        || state == QStringLiteral("disconnecting")) {
        m_mqttService.disconnectCurrentSession();
        return;
    }

    m_mqttService.connectCurrentSession();
}

void WorkbenchViewModel::refreshSubscriptionEditorProcessorOptions()
{
    QVariantList options;
    for (int row = 0; row < m_processorsModel.rowCount(); ++row) {
        options.append(m_processorsModel.rowAt(row));
    }
    m_subscriptionEditor.setProcessorOptions(options);
}

bool WorkbenchViewModel::openSubscriptionEditorForCreate(const QString &sessionId, const QString &topic)
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session || sessionId.isEmpty() || session->id != sessionId) {
        return false;
    }
    m_subscriptionEditorSessionId = sessionId;
    refreshSubscriptionEditorProcessorOptions();
    m_subscriptionEditor.openForCreate(topic);
    return true;
}

bool WorkbenchViewModel::openSubscriptionEditorForEdit(const QString &sessionId, const QString &topic)
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session || sessionId.isEmpty() || session->id != sessionId || topic.isEmpty()) {
        return false;
    }

    for (const SubscriptionEntry &subscription : session->subscriptions) {
        if (subscription.topic == topic) {
            m_subscriptionEditorSessionId = sessionId;
            refreshSubscriptionEditorProcessorOptions();
            m_subscriptionEditor.openForEdit(subscription);
            return true;
        }
    }
    return false;
}

bool WorkbenchViewModel::submitSubscriptionEditor()
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session || m_subscriptionEditorSessionId.isEmpty()
        || session->id != m_subscriptionEditorSessionId
        || !m_subscriptionEditor.canSubmit()) {
        return false;
    }

    const SubscriptionEditorSubmission submission = m_subscriptionEditor.submission();
    if (submission.editMode) {
        return m_subscriptionService.updateCurrentSubscription(
            submission.editTopic,
            submission.topic,
            submission.alias,
            submission.qos,
            submission.format,
            submission.processor,
            submission.color,
            submission.options);
    }

    return m_subscriptionService.upsertCurrentSubscriptions(
        submission.topics,
        submission.qos,
        submission.format,
        submission.processor,
        submission.color,
        submission.alias,
        submission.options);
}

void WorkbenchViewModel::requestSubscriptionDelete(const QString &topic, const QString &displayName)
{
    if (m_pendingSubscriptionDeleteTopic == topic && m_pendingSubscriptionDeleteDisplayName == displayName) {
        emit subscriptionDeleteRequested(topic, displayName);
        return;
    }

    m_pendingSubscriptionDeleteTopic = topic;
    m_pendingSubscriptionDeleteDisplayName = displayName;
    emit pendingSubscriptionDeleteChanged();
    emit subscriptionDeleteRequested(topic, displayName);
}

void WorkbenchViewModel::cancelPendingSubscriptionDelete()
{
    clearPendingSubscriptionDelete();
}

bool WorkbenchViewModel::confirmPendingSubscriptionDelete()
{
    const QString topic = m_pendingSubscriptionDeleteTopic;
    if (topic.isEmpty()) {
        clearPendingSubscriptionDelete();
        return false;
    }

    m_subscriptionService.removeCurrentSubscription(topic);
    clearPendingSubscriptionDelete();
    return true;
}

void WorkbenchViewModel::copyMessageTopic(const QString &topic) const
{
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(topic);
    }
}

QString WorkbenchViewModel::messagePayloadForDisplay(
    const QString &historyId,
    const QString &fallbackPayload,
    int format) const
{
    bool ok = false;
    const qint64 parsedHistoryId = historyId.toLongLong(&ok);
    return m_eventHistoryService.messagePayloadForDisplay(
        ok ? parsedHistoryId : 0,
        fallbackPayload,
        format);
}

bool WorkbenchViewModel::requestExpandedMessage(const QString &historyId)
{
    bool ok = false;
    const qint64 parsedHistoryId = historyId.toLongLong(&ok);
    return ok && m_eventHistoryService.requestExpandedMessage(parsedHistoryId);
}

void WorkbenchViewModel::copyMessagePayload(
    const QString &historyId,
    const QString &payload,
    const QString &testPayload,
    int format) const
{
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(reusableMessagePayload(historyId, payload, testPayload, format));
    }
}

void WorkbenchViewModel::useMessageAsDraft(
    const QString &historyId,
    const QString &topic,
    const QString &payload,
    const QString &testPayload,
    int format)
{
    m_publisher.useMessageAsDraft(
        topic,
        reusableMessagePayload(historyId, payload, testPayload, format),
        QString(),
        format);
}

void WorkbenchViewModel::setMessageSearchText(const QString &text)
{
    m_filteredMessagesModel.setFilterText(text);
}

void WorkbenchViewModel::clearMessageFilters()
{
    m_filteredMessagesModel.setFilterText({});
    m_filteredMessagesModel.setDirection(QStringLiteral("all"));
}

bool WorkbenchViewModel::setCurrentMessageCapturePolicy(
    bool captureIncoming,
    bool captureOutgoing,
    const QString &includeTopicFilters,
    const QString &excludeTopicFilters)
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session) {
        return false;
    }

    MessageCapturePolicy policy;
    policy.captureIncoming = captureIncoming;
    policy.captureOutgoing = captureOutgoing;
    policy.includeTopicFilters = topicFiltersFromText(includeTopicFilters);
    policy.excludeTopicFilters = topicFiltersFromText(excludeTopicFilters);
    return m_sessionService.setMessageCapturePolicy(session->id, policy);
}

QVariantMap WorkbenchViewModel::messageDetails(const QString &historyId) const
{
    bool ok = false;
    const qint64 id = historyId.toLongLong(&ok);
    if (!ok || id <= 0) {
        return {};
    }
    return m_eventHistoryService.messageDetails(id);
}

QString WorkbenchViewModel::reusableMessagePayload(
    const QString &historyId,
    const QString &payload,
    const QString &testPayload,
    int format) const
{
    bool ok = false;
    const qint64 parsedHistoryId = historyId.toLongLong(&ok);
    return m_eventHistoryService.messagePayloadForReuse(
        ok ? parsedHistoryId : 0,
        payload,
        testPayload,
        format);
}

void WorkbenchViewModel::clearPendingSubscriptionDelete()
{
    if (m_pendingSubscriptionDeleteTopic.isEmpty() && m_pendingSubscriptionDeleteDisplayName.isEmpty()) {
        return;
    }

    m_pendingSubscriptionDeleteTopic.clear();
    m_pendingSubscriptionDeleteDisplayName.clear();
    emit pendingSubscriptionDeleteChanged();
}
