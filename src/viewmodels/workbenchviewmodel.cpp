#include "viewmodels/workbenchviewmodel.h"

#include "usecases/eventhistoryservice.h"
#include "usecases/mqttsessionservice.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"
#include "domain/publishstatus.h"
#include "domain/sessionconfig.h"
#include "services/apputils.h"
#include "services/messaging/messagecapturepolicy.h"
#include "services/payload/payloadcodec.h"

#include <QClipboard>
#include <QCborValue>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QRegularExpression>

#include <algorithm>

using namespace AppUtils;

namespace {
constexpr int kDisplayTotalMessageCountIntervalMs = 50;
constexpr int kTrafficRateIntervalMs = 1000;

QStringList topicFiltersFromText(const QString &text)
{
    return text.split(QRegularExpression(QStringLiteral("[,\\r\\n]+")), Qt::SkipEmptyParts);
}

ProcessorReference processorReferenceFromSubmission(const QVariantMap &submission)
{
    ProcessorReference reference;
    reference.processorId = submission.value(QStringLiteral("processorId")).toString().trimmed();
    const QByteArray parametersCbor = QByteArray::fromBase64(
        submission.value(QStringLiteral("processorParametersCborBase64"))
            .toString()
            .toLatin1());
    if (!parametersCbor.isEmpty()) {
        QCborParserError error;
        const QCborValue parameters = QCborValue::fromCbor(parametersCbor, &error);
        if (error.error == QCborError::NoError && parameters.isMap()) {
            reference.parameters = parameters.toMap();
        }
    }
    return reference;
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
    SubscriptionFilterModel &messageFilterSubscriptionsModel,
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
    , m_messageFilterSubscriptionsModel(messageFilterSubscriptionsModel)
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
            &WorkbenchViewModel::messagePressureChanged);
    connect(&m_eventHistoryService,
            &EventHistoryService::messageRowsAppended,
            this,
            [this](const QVariantList &rows) {
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
    connect(&m_messageFilterSubscriptionsModel,
            &QAbstractItemModel::modelReset,
            this,
            &WorkbenchViewModel::messageTopicFilterStateChanged);
    connect(&m_messageFilterSubscriptionsModel,
            &QAbstractItemModel::dataChanged,
            this,
            &WorkbenchViewModel::messageTopicFilterStateChanged);
    connect(&m_messageFilterSubscriptionsModel,
            &QAbstractItemModel::rowsInserted,
            this,
            &WorkbenchViewModel::messageTopicFilterStateChanged);
    connect(&m_messageFilterSubscriptionsModel,
            &QAbstractItemModel::rowsRemoved,
            this,
            &WorkbenchViewModel::messageTopicFilterStateChanged);
    connect(&m_filteredMessagesModel,
            &MessageFilterModel::selectedTopicsChanged,
            this,
            &WorkbenchViewModel::messageTopicFilterStateChanged);
    m_displayTotalMessageCount = totalMessageCount();
    refreshTrafficRates();
    refreshSubscriptionEditorProcessorOptions();
}

SessionListModel *WorkbenchViewModel::sessions() const { return &m_sessionsModel; }
SubscriptionFilterModel *WorkbenchViewModel::filteredSubscriptions() const { return &m_filteredSubscriptionsModel; }
SubscriptionFilterModel *WorkbenchViewModel::messageFilterSubscriptions() const { return &m_messageFilterSubscriptionsModel; }
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
    row.insert(QStringLiteral("transportLabel"), transportLabel(session->transport));
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
                      .arg(transportLabel(session->transport));
        if (session->runtime.sessionRestored) {
            summary.append(QCoreApplication::translate("WorkbenchViewModel", " • session restored"));
        }
    } else if (state == QStringLiteral("connecting")) {
        summary = QCoreApplication::translate("WorkbenchViewModel", "Connecting to %1:%2 over %3")
                      .arg(client ? client->hostname() : QString())
                      .arg(client ? client->port() : SessionConfig::kDefaultPort)
                      .arg(transportLabel(session->transport));
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
    row.insert(QStringLiteral("transportLabel"), transportLabel(session->transport));
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

QVariantMap WorkbenchViewModel::messageTopicFilterState() const
{
    const QStringList selectedTopics = m_filteredMessagesModel.selectedTopics();
    int pausedCount = 0;
    QString singleTopicLabel;

    for (const QString &selectedTopic : selectedTopics) {
        QString displayName = selectedTopic;
        bool paused = false;
        for (int row = 0; row < m_messageFilterSubscriptionsModel.rowCount(); ++row) {
            const QVariantMap subscription = m_messageFilterSubscriptionsModel.rowAt(row);
            if (subscription.value(QStringLiteral("topic")).toString() != selectedTopic) {
                continue;
            }
            displayName = subscription.value(QStringLiteral("displayName")).toString();
            paused = subscription.value(QStringLiteral("paused")).toBool();
            break;
        }
        pausedCount += paused ? 1 : 0;
        if (selectedTopics.size() == 1) {
            singleTopicLabel = displayName;
        }
    }

    return {
        {QStringLiteral("selectedCount"), selectedTopics.size()},
        {QStringLiteral("pausedCount"), pausedCount},
        {QStringLiteral("singleTopicLabel"), singleTopicLabel},
    };
}

QVariantMap WorkbenchViewModel::messagePressure() const
{
    QVariantMap pressure;
    pressure.insert(QStringLiteral("state"), m_eventHistoryService.messagePressureState());
    pressure.insert(QStringLiteral("captureMode"), m_eventHistoryService.messageCaptureMode());
    pressure.insert(QStringLiteral("writerState"), m_eventHistoryService.messageWriterPressureState());
    pressure.insert(QStringLiteral("parserState"), m_eventHistoryService.messageParserPressureState());
    pressure.insert(QStringLiteral("backlog"), m_eventHistoryService.messageWriterBacklog());
    pressure.insert(QStringLiteral("backlogBytes"), m_eventHistoryService.messageWriterBacklogBytes());
    pressure.insert(QStringLiteral("dropped"), m_eventHistoryService.droppedMessageCount());
    pressure.insert(QStringLiteral("parseBacklog"), m_eventHistoryService.messageParserBacklog());
    pressure.insert(QStringLiteral("parseBacklogBytes"), m_eventHistoryService.messageParserBacklogBytes());
    pressure.insert(QStringLiteral("parseDropped"), m_eventHistoryService.droppedParseTaskCount());
    pressure.insert(QStringLiteral("parseResultDropped"), m_eventHistoryService.droppedParseResultCount());
    pressure.insert(QStringLiteral("captureFiltered"), m_eventHistoryService.captureFilteredMessageCount());
    pressure.insert(QStringLiteral("parseSkippedPressure"), m_eventHistoryService.pressureSkippedParseCount());
    pressure.insert(QStringLiteral("storageDegraded"), m_eventHistoryService.messageStorageDegraded());
    pressure.insert(QStringLiteral("lastError"), m_eventHistoryService.messageStorageError());
    return pressure;
}

QVariantMap WorkbenchViewModel::messageCapturePolicy() const
{
    const SessionState *session = m_sessionService.currentSession();
    const MessageCapturePolicy policy = session
        ? m_eventHistoryService.messageCapturePolicy(session->id)
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
        ? recentTrafficByteCount(session->runtime.recentReceivedTraffic, nowMs)
        : 0;
    const qint64 outgoingRate = session
        ? recentTrafficByteCount(session->runtime.recentPublishedTraffic, nowMs)
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

    return recentTrafficSampleCount(
        session->runtime.recentReceivedTraffic,
        QDateTime::currentMSecsSinceEpoch());
}

qreal WorkbenchViewModel::currentOutgoingMessageRate() const
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session) {
        return 0;
    }
    return recentTrafficSampleCount(
        session->runtime.recentPublishedTraffic,
        QDateTime::currentMSecsSinceEpoch());
}

qint64 WorkbenchViewModel::currentIncomingByteRate() const
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session) {
        return 0;
    }

    return recentTrafficByteCount(
        session->runtime.recentReceivedTraffic,
        QDateTime::currentMSecsSinceEpoch());
}

qint64 WorkbenchViewModel::currentOutgoingByteRate() const
{
    const SessionState *session = m_sessionService.currentSession();
    if (!session) {
        return 0;
    }

    return recentTrafficByteCount(
        session->runtime.recentPublishedTraffic,
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

void WorkbenchViewModel::openSessionEditorForEdit(int index)
{
    if (index < 0 || index >= m_sessionsModel.rowCount()) {
        return;
    }
    m_sessionEditor.openForEdit(index, m_sessionService.sessionConfigAt(index));
}

bool WorkbenchViewModel::submitSessionEditor()
{
    if (!m_sessionEditor.validate()) {
        return false;
    }
    const QVariantMap config = m_sessionEditor.collectedConfig();
    if (!m_sessionEditor.editMode()) {
        return m_sessionService.addSessionWithConfig(config);
    }

    const int index = m_sessionEditor.targetIndex();
    return index >= 0 && m_sessionService.updateSessionConfigAt(index, config);
}

void WorkbenchViewModel::requestSessionDuplicate(int index)
{
    if (index < 0 || index >= m_sessionsModel.rowCount()) {
        return;
    }

    m_sessionService.duplicateSessionAt(index);
}

void WorkbenchViewModel::requestSessionDelete(int index)
{
    if (index < 0 || index >= m_sessionsModel.rowCount() || m_sessionsModel.rowCount() <= 1) {
        return;
    }

    m_sessionService.removeSessionAt(index);
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

void WorkbenchViewModel::openSubscriptionEditorForCreate()
{
    refreshSubscriptionEditorProcessorOptions();
    m_subscriptionEditor.openForCreate();
}

bool WorkbenchViewModel::openSubscriptionEditorForEdit(int filteredIndex)
{
    if (filteredIndex < 0 || filteredIndex >= m_filteredSubscriptionsModel.rowCount()) {
        return false;
    }

    refreshSubscriptionEditorProcessorOptions();
    m_subscriptionEditor.openForEdit(m_filteredSubscriptionsModel.rowAt(filteredIndex));
    return true;
}

bool WorkbenchViewModel::submitSubscriptionEditor()
{
    if (!m_subscriptionEditor.canSubmit()) {
        return false;
    }

    const QVariantMap submission = m_subscriptionEditor.submission();
    const ProcessorReference processor = processorReferenceFromSubmission(submission);
    if (submission.value(QStringLiteral("editMode")).toBool()) {
        return m_subscriptionService.updateCurrentSubscription(
            submission.value(QStringLiteral("editTopic")).toString(),
            submission.value(QStringLiteral("topic")).toString(),
            submission.value(QStringLiteral("alias")).toString(),
            submission.value(QStringLiteral("qos")).toInt(),
            submission.value(QStringLiteral("format")).toInt(),
            processor,
            submission.value(QStringLiteral("color")).toString());
    }

    return m_subscriptionService.upsertCurrentSubscription(
        submission.value(QStringLiteral("topic")).toString(),
        submission.value(QStringLiteral("qos")).toInt(),
        submission.value(QStringLiteral("format")).toInt(),
        processor,
        submission.value(QStringLiteral("color")).toString(),
        submission.value(QStringLiteral("alias")).toString());
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

void WorkbenchViewModel::setMessageTopicFilter(const QString &topic)
{
    const QString trimmed = topic.trimmed();
    m_filteredMessagesModel.setSelectedTopics(
        trimmed.isEmpty() ? QStringList {} : QStringList {trimmed});
}

void WorkbenchViewModel::setMessageSearchText(const QString &text)
{
    m_filteredMessagesModel.setFilterText(text);
}

void WorkbenchViewModel::addMessageTopicFilter(const QString &topic)
{
    const QString trimmed = topic.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    QStringList topics = m_filteredMessagesModel.selectedTopics();
    if (!topics.contains(trimmed)) {
        topics.append(trimmed);
        m_filteredMessagesModel.setSelectedTopics(topics);
    }
}

void WorkbenchViewModel::clearMessageFilters()
{
    m_filteredMessagesModel.setFilterText({});
    m_filteredMessagesModel.setSelectedTopics({});
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
    return m_eventHistoryService.setMessageCapturePolicy(session->id, policy);
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
