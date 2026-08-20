#include "viewmodels/workbenchviewmodel.h"
#include "viewmodels/draftsviewmodel.h"

#include "domain/session.h"
#include "models/draftlibrarymodel.h"
#include "models/processorlibrarymodel.h"
#include "models/subscriptionlistmodel.h"
#include "models/topictreemodel.h"
#include "domain/messagecapturepolicy.h"
#include "services/storage/historystore.h"
#include "services/storage/historywriterworker.h"
#include "services/parsing/messageparseworker.h"
#include "services/processors/processorlibrary.h"
#include "usecases/eventhistoryservice.h"
#include "usecases/draftlibraryservice.h"
#include "usecases/mqttsessionservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"

#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {

int failingSettingsWriteCount = 0;

bool readEmptySettings(QIODevice &, QSettings::SettingsMap &settings)
{
    settings.clear();
    return true;
}

bool rejectSettingsWrite(QIODevice &, const QSettings::SettingsMap &)
{
    ++failingSettingsWriteCount;
    return false;
}

QSettings::Format failingSettingsFormat()
{
    static const QSettings::Format format = QSettings::registerFormat(
        QStringLiteral("mqtt-plus-failing-settings"),
        readEmptySettings,
        rejectSettingsWrite);
    return format;
}

struct WorkbenchFixture
{
    explicit WorkbenchFixture(QSettings::Format settingsFormat = QSettings::IniFormat)
        : settings(temporaryDirectory.filePath(QStringLiteral("settings.data")), settingsFormat)
        , preferences(&settings)
        , historyStore(temporaryDirectory.path())
        , historyWriter(temporaryDirectory.path(), historyStore.nextMessageId())
        , processorLibrary(temporaryDirectory.filePath(QStringLiteral("processors")))
        , sessionService(
              settings,
              historyStore,
              preferences)
        , eventHistoryService(
              sessionService,
              historyStore,
              historyWriter,
              messageParser,
              messagesModel,
              logsModel,
              processorLibrary,
              launchTimestamp,
              preferences)
        , subscriptionService(
              sessionService,
              eventHistoryService)
        , mqttService(
              sessionService,
              subscriptionService,
              eventHistoryService)
        , draftService(temporaryDirectory.filePath(QStringLiteral("drafts")))
        , viewModel(
              sessionService,
              mqttService,
              draftService,
              draftsModel,
              subscriptionService,
              eventHistoryService,
              sessionsModel,
              filteredSubscriptionsModel,
              messageFilterSubscriptionsModel,
              topicTreeModel,
              messagesModel,
              filteredMessagesModel,
              processorsModel)
    {
        historyWriter.start();
        messageParser.start();
        sessionService.setHistoryWriter(&historyWriter);
        sessionService.setMessageParser(&messageParser);
        sessionsModel.setSessions(sessionService.sessions());
        subscriptionsModel.setSubscriptions(QString(), {}, &processorLibrary);
        filteredSubscriptionsModel.setSourceModel(&subscriptionsModel);
        messageFilterSubscriptionsModel.setSourceModel(&subscriptionsModel);
        filteredMessagesModel.setSourceModel(&messagesModel);
    }

    QTemporaryDir temporaryDirectory;
    QSettings settings;
    PreferencesController preferences;
    HistoryStore historyStore;
    HistoryWriterWorker historyWriter;
    MessageParseWorker messageParser;
    ProcessorLibrary processorLibrary;
    SessionService sessionService;
    SessionListModel sessionsModel;
    SubscriptionListModel subscriptionsModel;
    SubscriptionFilterModel filteredSubscriptionsModel;
    SubscriptionFilterModel messageFilterSubscriptionsModel;
    TopicTreeModel topicTreeModel;
    EventStreamModel messagesModel;
    EventStreamModel logsModel;
    MessageFilterModel filteredMessagesModel;
    ProcessorLibraryModel processorsModel;
    QString launchTimestamp = QStringLiteral("2026-07-25T00:00:00.000");
    EventHistoryService eventHistoryService;
    SubscriptionService subscriptionService;
    MqttSessionService mqttService;
    DraftLibraryService draftService;
    DraftLibraryModel draftsModel;
    WorkbenchViewModel viewModel;
};

} // namespace

class WorkbenchViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesDefaultPublishDraft();
    void exposesMessagePressureState();
    void persistsAndDuplicatesMessageCapturePolicy();
    void rollsBackMessageCapturePolicyWhenSettingsWriteFails();
    void recoversFromInvalidSessionSettingsWithoutOverwritingFile();
    void rejectsSessionCreateWhenSettingsWriteFails();
    void rollsBackSessionEditWhenSettingsWriteFails();
    void exposesSessionEditor();
    void exposesSubscriptionEditor();
    void exposesTopicTree();
    void preparesSubscriptionEditorForCreate();
    void addsBatchSubscriptions();
    void rejectsInvalidSubscriptionEditorIndex();
    void opensFilteredSubscriptionEditorWithTypedEntry();
    void ignoresInvalidSessionIndexes();
    void updatesPublishDraft();
    void savedDraftQuickPublishDoesNotReplaceComposer();
    void draftEditorSelectionFollowsPopulatedModel();
    void failedEditorSaveDoesNotConsumeComposerSave();
    void rejectsPublishWithoutConnectedSession();
    void forwardsSessionAndRuntimeStateNotificationsSeparately();
    void forwardsMessageBatchNotifications();
    void exposesTotalMessageCount();
    void coalescesDisplayTotalMessageCountUpdates();
    void exposesConnectionTimingAndAggregateRates();
    void ownsSubscriptionFilterState();
    void ownsPendingSubscriptionDeleteState();
    void handlesIntentCommandsWithoutCurrentSession();
    void ownsMessageFilterState();
    void exposesUnfilteredSubscriptionsAndSelectedTopicState();
};

void WorkbenchViewModelTest::exposesDefaultPublishDraft()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;

    QVERIFY(viewModel.publisher());
    QCOMPARE(viewModel.publisher()->topic(), QString());
    QCOMPARE(viewModel.publisher()->payload(), QString());
    QCOMPARE(viewModel.publisher()->format(), 1);
    QCOMPARE(viewModel.publisher()->qos(), 0);
    QCOMPARE(viewModel.publisher()->retain(), false);
    QVERIFY(!viewModel.publisher()->canPublish());
}

void WorkbenchViewModelTest::exposesMessagePressureState()
{
    WorkbenchFixture fixture;
    QVariantMap pressure = fixture.viewModel.messagePressure();
    QCOMPARE(pressure.value(QStringLiteral("state")).toString(), QStringLiteral("normal"));
    QCOMPARE(pressure.value(QStringLiteral("captureMode")).toString(), QStringLiteral("full"));
    QCOMPARE(pressure.value(QStringLiteral("writerState")).toString(), QStringLiteral("normal"));
    QCOMPARE(pressure.value(QStringLiteral("parserState")).toString(), QStringLiteral("normal"));
    QCOMPARE(pressure.value(QStringLiteral("backlog")).toInt(), 0);
    QCOMPARE(pressure.value(QStringLiteral("dropped")).toLongLong(), 0);
    QCOMPARE(pressure.value(QStringLiteral("parseBacklog")).toInt(), 0);
    QCOMPARE(pressure.value(QStringLiteral("parseDropped")).toLongLong(), 0);
    QCOMPARE(pressure.value(QStringLiteral("parseResultDropped")).toLongLong(), 0);
    QCOMPARE(pressure.value(QStringLiteral("captureFiltered")).toLongLong(), 0);
    QCOMPARE(pressure.value(QStringLiteral("parseSkippedPressure")).toLongLong(), 0);
    QCOMPARE(pressure.value(QStringLiteral("storageDegraded")).toBool(), false);

    MessageRecord record;
    record.sessionId = QStringLiteral("session-1");
    record.timestamp = QStringLiteral("2026-08-01T10:00:00.000Z");
    record.topic = QStringLiteral("devices/pressure");
    record.payloadBytes = QByteArrayLiteral("payload");
    record.payloadSize = record.payloadBytes.size();
    record.payloadPreview = QStringLiteral("payload");
    QSignalSpy pressureSpy(&fixture.viewModel, &WorkbenchViewModel::messagePressureChanged);

    QVERIFY(fixture.historyWriter.enqueueMessage(record) > 0);
    QTRY_COMPARE(pressureSpy.count(), 1);
    pressure = fixture.viewModel.messagePressure();
    QCOMPARE(pressure.value(QStringLiteral("backlog")).toInt(), 1);
    QCOMPARE(pressure.value(QStringLiteral("backlogBytes")).toLongLong() > 0, true);
}

void WorkbenchViewModelTest::persistsAndDuplicatesMessageCapturePolicy()
{
    WorkbenchFixture fixture;
    QVERIFY(fixture.sessionService.loadSessions());
    fixture.sessionService.setCurrentSessionIndex(0);
    fixture.sessionsModel.setSessions(fixture.sessionService.sessions());
    QSignalSpy policySpy(
        &fixture.viewModel,
        &WorkbenchViewModel::messageCapturePolicyChanged);

    QVERIFY(fixture.viewModel.setCurrentMessageCapturePolicy(
        true,
        false,
        QStringLiteral(" alerts/#, devices/+/state, alerts/# "),
        QStringLiteral("devices/private/#\n diagnostics/+ ")));

    const QVariantMap exposedPolicy = fixture.viewModel.messageCapturePolicy();
    QCOMPARE(exposedPolicy.value(QStringLiteral("captureIncoming")).toBool(), true);
    QCOMPARE(exposedPolicy.value(QStringLiteral("captureOutgoing")).toBool(), false);
    QCOMPARE(
        exposedPolicy.value(QStringLiteral("includeTopicFilters")).toStringList(),
        QStringList({QStringLiteral("alerts/#"), QStringLiteral("devices/+/state")}));
    QCOMPARE(
        exposedPolicy.value(QStringLiteral("excludeTopicFilters")).toStringList(),
        QStringList({QStringLiteral("devices/private/#"), QStringLiteral("diagnostics/+")}));
    QCOMPARE(policySpy.size(), 1);

    const QString sourceSessionId = fixture.sessionService.currentSession()->id;
    fixture.viewModel.requestSessionDuplicate(0);
    QCOMPARE(fixture.sessionService.sessions().size(), 2);
    const QString duplicateSessionId = fixture.sessionService.currentSession()->id;
    const SessionState *sourceSession = fixture.sessionService.sessionById(sourceSessionId);
    const SessionState *duplicateSession = fixture.sessionService.sessionById(duplicateSessionId);
    QVERIFY(sourceSession);
    QVERIFY(duplicateSession);
    const MessageCapturePolicy sourcePolicy = sourceSession->capturePolicy;
    const MessageCapturePolicy duplicatePolicy = duplicateSession->capturePolicy;
    QCOMPARE(duplicatePolicy.captureIncoming, sourcePolicy.captureIncoming);
    QCOMPARE(duplicatePolicy.captureOutgoing, sourcePolicy.captureOutgoing);
    QCOMPARE(duplicatePolicy.includeTopicFilters, sourcePolicy.includeTopicFilters);
    QCOMPARE(duplicatePolicy.excludeTopicFilters, sourcePolicy.excludeTopicFilters);

    QSettings reloadedSettings(fixture.settings.fileName(), QSettings::IniFormat);
    PreferencesController reloadedPreferences(&reloadedSettings);
    SessionService reloadedService(
        reloadedSettings,
        fixture.historyStore,
        reloadedPreferences);
    QVERIFY(reloadedService.loadSessions());
    QCOMPARE(reloadedService.sessions().size(), 2);
    const SessionState *reloadedSession = reloadedService.sessionById(sourceSessionId);
    QVERIFY(reloadedSession);
    const MessageCapturePolicy reloadedPolicy = reloadedSession->capturePolicy;
    QCOMPARE(reloadedPolicy.captureIncoming, true);
    QCOMPARE(reloadedPolicy.captureOutgoing, false);
    QCOMPARE(reloadedPolicy.includeTopicFilters, sourcePolicy.includeTopicFilters);
    QCOMPARE(reloadedPolicy.excludeTopicFilters, sourcePolicy.excludeTopicFilters);
}

void WorkbenchViewModelTest::rollsBackMessageCapturePolicyWhenSettingsWriteFails()
{
    const QSettings::Format format = failingSettingsFormat();
    QVERIFY(format != QSettings::InvalidFormat);
    WorkbenchFixture fixture(format);
    QVERIFY(!fixture.sessionService.loadSessions());
    fixture.sessionService.setCurrentSessionIndex(0);
    fixture.sessionsModel.setSessions(fixture.sessionService.sessions());
    const QVariantMap previousPolicy = fixture.viewModel.messageCapturePolicy();
    QSignalSpy storageErrorSpy(&fixture.sessionService, &SessionService::storageError);
    QSignalSpy sessionsChangedSpy(&fixture.sessionService, &SessionService::sessionsChanged);
    QSignalSpy policySpy(
        &fixture.viewModel,
        &WorkbenchViewModel::messageCapturePolicyChanged);
    failingSettingsWriteCount = 0;

    QVERIFY(!fixture.viewModel.setCurrentMessageCapturePolicy(
        false,
        false,
        QStringLiteral("alerts/#"),
        QStringLiteral("alerts/private/#")));

    QCOMPARE(fixture.viewModel.messageCapturePolicy(), previousPolicy);
    QCOMPARE(storageErrorSpy.size(), 1);
    QCOMPARE(sessionsChangedSpy.size(), 0);
    QCOMPARE(policySpy.size(), 0);
    QCOMPARE(failingSettingsWriteCount, 2);
}

void WorkbenchViewModelTest::recoversFromInvalidSessionSettingsWithoutOverwritingFile()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    const QString settingsPath = dataDir.filePath(QStringLiteral("settings.ini"));
    QFile settingsFile(settingsPath);
    const bool openedForWrite = settingsFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    QVERIFY(openedForWrite);
    QCOMPARE(settingsFile.write("["), 1);
    settingsFile.close();

    QSettings settings(settingsPath, QSettings::IniFormat);
    PreferencesController preferences(&settings);
    HistoryStore historyStore(dataDir.path());
    SessionService sessionService(settings, historyStore, preferences);
    QSignalSpy errorSpy(&sessionService, &SessionService::storageError);

    QVERIFY(!sessionService.loadSessions());
    QCOMPARE(sessionService.sessions().size(), 1);
    QCOMPARE(sessionService.sessions().front().name, QStringLiteral("Session 1"));
    QCOMPARE(errorSpy.size(), 1);
    QCOMPARE(
        errorSpy.front().front().toString(),
        QStringLiteral("Cannot read session settings: invalid settings format."));

    const bool openedForRead = settingsFile.open(QIODevice::ReadOnly);
    QVERIFY(openedForRead);
    QCOMPARE(settingsFile.readAll(), QByteArray("["));
}

void WorkbenchViewModelTest::rejectsSessionCreateWhenSettingsWriteFails()
{
    const QSettings::Format format = failingSettingsFormat();
    QVERIFY(format != QSettings::InvalidFormat);
    WorkbenchFixture fixture(format);
    QVERIFY(!fixture.sessionService.loadSessions());
    fixture.sessionService.setCurrentSessionIndex(0);
    fixture.sessionsModel.setSessions(fixture.sessionService.sessions());

    const int previousCount = fixture.sessionService.sessions().size();
    const int previousIndex = fixture.sessionService.currentIndex();
    QSignalSpy storageErrorSpy(&fixture.sessionService, &SessionService::storageError);
    QSignalSpy runtimeReadySpy(&fixture.sessionService, &SessionService::sessionRuntimeReady);
    QSignalSpy sessionsChangedSpy(&fixture.sessionService, &SessionService::sessionsChanged);
    QSignalSpy indexChangedSpy(&fixture.sessionService, &SessionService::currentSessionIndexChanged);
    QSignalSpy currentChangedSpy(&fixture.sessionService, &SessionService::currentSessionChanged);
    failingSettingsWriteCount = 0;

    fixture.viewModel.openSessionEditorForCreate();
    fixture.viewModel.sessionEditor()->setName(QStringLiteral("Unsaved Session"));

    QVERIFY(!fixture.viewModel.submitSessionEditor());
    QCOMPARE(fixture.sessionService.sessions().size(), previousCount);
    QCOMPARE(fixture.sessionService.currentIndex(), previousIndex);
    QCOMPARE(storageErrorSpy.size(), 1);
    QCOMPARE(runtimeReadySpy.size(), 0);
    QCOMPARE(sessionsChangedSpy.size(), 0);
    QCOMPARE(indexChangedSpy.size(), 0);
    QCOMPARE(currentChangedSpy.size(), 0);
    QCOMPARE(failingSettingsWriteCount, 2);
}

void WorkbenchViewModelTest::rollsBackSessionEditWhenSettingsWriteFails()
{
    const QSettings::Format format = failingSettingsFormat();
    QVERIFY(format != QSettings::InvalidFormat);
    WorkbenchFixture fixture(format);
    QVERIFY(!fixture.sessionService.loadSessions());
    fixture.sessionService.setCurrentSessionIndex(0);
    fixture.sessionsModel.setSessions(fixture.sessionService.sessions());

    SessionState &session = fixture.sessionService.sessions().front();
    QVERIFY(session.runtime.client);
    session.runtime.lastError = QStringLiteral("Existing runtime error");
    session.runtime.sessionRestored = true;
    session.runtime.publishStatus.state = QStringLiteral("sent");
    session.runtime.publishStatus.topic = QStringLiteral("devices/status");
    session.runtime.publishStatus.reason = QStringLiteral("Existing publish status");
    session.runtime.publishStatus.updatedAt = QStringLiteral("2026-07-25T12:00:00.000Z");
    const SessionConnectionConfig previousConfig = fixture.sessionService.sessionConfigAt(0);
    const QVariantMap previousPublishStatus = session.runtime.publishStatus.toVariantMap();
    QSignalSpy storageErrorSpy(&fixture.sessionService, &SessionService::storageError);
    QSignalSpy sessionsChangedSpy(&fixture.sessionService, &SessionService::sessionsChanged);
    QSignalSpy currentChangedSpy(&fixture.sessionService, &SessionService::currentSessionChanged);
    failingSettingsWriteCount = 0;

    fixture.viewModel.openSessionEditorForEdit(0);
    fixture.viewModel.sessionEditor()->setName(QStringLiteral("Changed Session"));
    fixture.viewModel.sessionEditor()->setHost(QStringLiteral("changed.example.com"));

    QVERIFY(!fixture.viewModel.submitSessionEditor());
    QVERIFY(fixture.sessionService.sessionConfigAt(0) == previousConfig);
    QCOMPARE(session.runtime.lastError, QStringLiteral("Existing runtime error"));
    QVERIFY(session.runtime.sessionRestored);
    QCOMPARE(session.runtime.publishStatus.toVariantMap(), previousPublishStatus);
    QCOMPARE(storageErrorSpy.size(), 1);
    QCOMPARE(sessionsChangedSpy.size(), 0);
    QCOMPARE(currentChangedSpy.size(), 0);
    QCOMPARE(failingSettingsWriteCount, 2);
}

void WorkbenchViewModelTest::exposesSessionEditor()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;

    QVERIFY(viewModel.sessionEditor());
    viewModel.openSessionEditorForCreate();
    QCOMPARE(viewModel.sessionEditor()->targetIndex(), -1);
    QCOMPARE(viewModel.sessionEditor()->title(), QStringLiteral("New Connection"));
    viewModel.sessionEditor()->setName(QString());

    QVERIFY(!viewModel.submitSessionEditor());
    QCOMPARE(viewModel.sessionEditor()->validationError(), QStringLiteral("Name is required."));
}

void WorkbenchViewModelTest::exposesSubscriptionEditor()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;

    QVERIFY(viewModel.subscriptionEditor());
    viewModel.subscriptionEditor()->openForCreate();
    viewModel.subscriptionEditor()->setTopic(QStringLiteral("devices/+/temp"));

    QVERIFY(!viewModel.submitSubscriptionEditor());
    QCOMPARE(viewModel.subscriptionEditor()->topic(), QStringLiteral("devices/+/temp"));
}

void WorkbenchViewModelTest::exposesTopicTree()
{
    WorkbenchFixture fixture;
    QCOMPARE(fixture.viewModel.topicTree(), &fixture.topicTreeModel);
}

void WorkbenchViewModelTest::preparesSubscriptionEditorForCreate()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;
    viewModel.subscriptionEditor()->setTopic(QStringLiteral("devices/+/temp"));
    viewModel.subscriptionEditor()->setAlias(QStringLiteral("Temperature"));

    viewModel.openSubscriptionEditorForCreate();

    QVERIFY(!viewModel.subscriptionEditor()->editMode());
    QVERIFY(viewModel.subscriptionEditor()->topic().isEmpty());
    QVERIFY(viewModel.subscriptionEditor()->alias().isEmpty());

    viewModel.openSubscriptionEditorForCreate(QStringLiteral("sensors/room/#"));
    QCOMPARE(viewModel.subscriptionEditor()->topic(), QStringLiteral("sensors/room/#"));
    QVERIFY(viewModel.subscriptionEditor()->canSubmit());
}

void WorkbenchViewModelTest::addsBatchSubscriptions()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;
    SessionState session;
    session.id = QStringLiteral("session-1");
    session.name = QStringLiteral("Session 1");
    fixture.sessionService.sessions().append(session);
    fixture.sessionService.setCurrentSessionIndex(0);

    viewModel.openSubscriptionEditorForCreate();
    viewModel.subscriptionEditor()->setTopic(QStringLiteral(
        "devices/one, devices/two\ndevices/one"));
    viewModel.subscriptionEditor()->setQos(1);
    viewModel.subscriptionEditor()->setFormat(2);

    QVERIFY(viewModel.submitSubscriptionEditor());
    const SessionState *currentSession = fixture.sessionService.currentSession();
    QVERIFY(currentSession);
    QCOMPARE(currentSession->subscriptions.size(), 2);
    QCOMPARE(currentSession->subscriptions.at(0).topic, QStringLiteral("devices/one"));
    QCOMPARE(currentSession->subscriptions.at(1).topic, QStringLiteral("devices/two"));
    QCOMPARE(currentSession->subscriptions.at(0).requestedQos, 1);
    QCOMPARE(currentSession->subscriptions.at(1).format, 2);
}

void WorkbenchViewModelTest::rejectsInvalidSubscriptionEditorIndex()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;

    QVERIFY(!viewModel.openSubscriptionEditorForEdit(0));
    QVERIFY(!viewModel.subscriptionEditor()->editMode());
}

void WorkbenchViewModelTest::opensFilteredSubscriptionEditorWithTypedEntry()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;

    SessionState session;
    session.id = QStringLiteral("session-1");
    session.name = QStringLiteral("Session 1");
    SubscriptionEntry hidden;
    hidden.topic = QStringLiteral("devices/hidden");
    session.subscriptions.append(hidden);
    SubscriptionEntry visible;
    visible.topic = QStringLiteral("devices/visible");
    visible.alias = QStringLiteral("Visible device");
    visible.requestedQos = 2;
    visible.format = 1;
    visible.processor.processorId = QStringLiteral("processor-1");
    visible.processor.parameters.insert(QStringLiteral("gain"), 4);
    visible.color = QStringLiteral("#34C759");
    session.subscriptions.append(visible);
    fixture.sessionService.sessions().append(session);
    fixture.sessionService.setCurrentSessionIndex(0);
    fixture.subscriptionsModel.setSubscriptions(
        session.id,
        session.subscriptions,
        &fixture.processorLibrary);
    fixture.filteredSubscriptionsModel.setFilterText(QStringLiteral("visible"));

    QCOMPARE(fixture.filteredSubscriptionsModel.rowCount(), 1);
    QVERIFY(viewModel.openSubscriptionEditorForEdit(0));
    QCOMPARE(viewModel.subscriptionEditor()->editTopic(), visible.topic);
    QCOMPARE(viewModel.subscriptionEditor()->alias(), visible.alias);
    QCOMPARE(viewModel.subscriptionEditor()->qos(), visible.requestedQos);
    QCOMPARE(viewModel.subscriptionEditor()->format(), visible.format);
    QCOMPARE(viewModel.subscriptionEditor()->processorId(), visible.processor.processorId);
    QCOMPARE(viewModel.subscriptionEditor()->color(), visible.color);
    QCOMPARE(
        viewModel.subscriptionEditor()
            ->submission()
            .processor.parameters.value(QStringLiteral("gain"))
            .toInteger(),
        4);
}

void WorkbenchViewModelTest::ignoresInvalidSessionIndexes()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;

    viewModel.requestSessionDuplicate(0);
    viewModel.requestSessionDelete(0);

    QVERIFY(fixture.sessionService.sessions().isEmpty());
}

void WorkbenchViewModelTest::updatesPublishDraft()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;
    auto *publisher = viewModel.publisher();
    QSignalSpy topicSpy(publisher, &PublishComposerViewModel::topicChanged);
    QSignalSpy payloadSpy(publisher, &PublishComposerViewModel::payloadChanged);
    QSignalSpy formatSpy(publisher, &PublishComposerViewModel::formatChanged);
    QSignalSpy qosSpy(publisher, &PublishComposerViewModel::qosChanged);
    QSignalSpy retainSpy(publisher, &PublishComposerViewModel::retainChanged);

    publisher->setTopic(QStringLiteral(" sensors/temp "));
    publisher->setPayload(QStringLiteral("{\"value\":23}"));
    publisher->setFormat(2);
    publisher->setQos(2);
    publisher->setRetain(true);
    publisher->useMessageAsDraft(QStringLiteral("devices/humidity"), QStringLiteral("raw"), QStringLiteral("decoded"), 0);

    QCOMPARE(publisher->topic(), QStringLiteral("devices/humidity"));
    QCOMPARE(publisher->payload(), QStringLiteral("decoded"));
    QCOMPARE(publisher->format(), 0);
    QCOMPARE(publisher->qos(), 2);
    QCOMPARE(publisher->retain(), true);
    QCOMPARE(topicSpy.size(), 2);
    QCOMPARE(payloadSpy.size(), 2);
    QCOMPARE(formatSpy.size(), 2);
    QCOMPARE(qosSpy.size(), 1);
    QCOMPARE(retainSpy.size(), 1);

    publisher->setQos(3);
    QCOMPARE(publisher->qos(), 2);
    QCOMPARE(qosSpy.size(), 1);
}

void WorkbenchViewModelTest::savedDraftQuickPublishDoesNotReplaceComposer()
{
    WorkbenchFixture fixture;
    fixture.draftService.load();
    QTRY_VERIFY(fixture.draftService.ready());

    PublishDraft draft;
    draft.name = QStringLiteral("Reset device");
    draft.defaultTopic = QStringLiteral("devices/reset");
    draft.payload = QStringLiteral("now");
    draft.formatId = QStringLiteral("text");
    draft.qos = 2;
    draft.retain = true;
    QVERIFY(fixture.draftService.createDraft(draft));
    QTRY_COMPARE(fixture.draftService.drafts().size(), 1);
    fixture.draftsModel.setDrafts(fixture.draftService.drafts());

    PublishComposerViewModel *publisher = fixture.viewModel.publisher();
    publisher->setTopic(QStringLiteral("composer/topic"));
    publisher->setPayload(QStringLiteral("composer payload"));
    publisher->setFormat(1);
    publisher->setQos(0);
    publisher->setRetain(false);
    QVERIFY(publisher->wouldReplaceWithDraft(0));

    QVERIFY(!publisher->quickPublishDraft(0));
    QCOMPARE(publisher->topic(), QStringLiteral("composer/topic"));
    QCOMPARE(publisher->payload(), QStringLiteral("composer payload"));
    QCOMPARE(publisher->format(), 1);
    QCOMPARE(publisher->qos(), 0);
    QVERIFY(!publisher->retain());

    QVERIFY(publisher->useSavedDraft(0));
    QCOMPARE(publisher->topic(), QStringLiteral("devices/reset"));
    QCOMPARE(publisher->payload(), QStringLiteral("now"));
    QCOMPARE(publisher->format(), 0);
    QCOMPARE(publisher->qos(), 2);
    QVERIFY(publisher->retain());
}

void WorkbenchViewModelTest::draftEditorSelectionFollowsPopulatedModel()
{
    WorkbenchFixture fixture;
    DraftsViewModel draftsViewModel(
        fixture.draftService,
        fixture.draftsModel);
    QObject::connect(
        &fixture.draftService,
        &DraftLibraryService::draftsChanged,
        &fixture.draftsModel,
        [&fixture]() { fixture.draftsModel.setDrafts(fixture.draftService.drafts()); });

    fixture.draftService.load();
    QTRY_VERIFY(fixture.draftService.ready());

    PublishDraft draft;
    draft.name = QStringLiteral("Selected after load");
    draft.formatId = QStringLiteral("text");
    QVERIFY(fixture.draftService.createDraft(draft));
    QTRY_COMPARE(fixture.draftService.drafts().size(), 1);
    QTRY_COMPARE(
        draftsViewModel.editor()->currentDraftId(),
        fixture.draftService.drafts().first().id);
}

void WorkbenchViewModelTest::failedEditorSaveDoesNotConsumeComposerSave()
{
    WorkbenchFixture fixture;
    DraftsViewModel draftsViewModel(
        fixture.draftService,
        fixture.draftsModel);
    fixture.draftService.load();
    QTRY_VERIFY(fixture.draftService.ready());

    const QString blockedRoot = fixture.temporaryDirectory.filePath(QStringLiteral("drafts"));
    QFile blocker(blockedRoot);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.write("x");
    blocker.close();

    draftsViewModel.editor()->setName(QStringLiteral("Unsaved editor draft"));
    QSignalSpy storageErrorSpy(&fixture.draftService, &DraftLibraryService::storageError);
    QVERIFY(draftsViewModel.saveEditor());
    QTRY_COMPARE(storageErrorSpy.size(), 1);
    QVERIFY(draftsViewModel.editor()->currentDraftId().isEmpty());

    QVERIFY(QFile::remove(blockedRoot));
    QVERIFY(fixture.viewModel.publisher()->saveAsDraft(QStringLiteral("Composer draft")));
    QTRY_COMPARE(fixture.draftService.drafts().size(), 1);

    QVERIFY(draftsViewModel.editor()->currentDraftId().isEmpty());
    QCOMPARE(draftsViewModel.editor()->name(), QStringLiteral("Unsaved editor draft"));
}

void WorkbenchViewModelTest::rejectsPublishWithoutConnectedSession()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;

    viewModel.publisher()->setTopic(QStringLiteral("sensors/temp"));
    viewModel.publisher()->setPayload(QStringLiteral("23"));

    QVERIFY(!viewModel.publisher()->canPublish());
    QVERIFY(!viewModel.publisher()->publishDraft());
    QCOMPARE(viewModel.publisher()->topic(), QStringLiteral("sensors/temp"));
    QCOMPARE(viewModel.publisher()->payload(), QStringLiteral("23"));
}

void WorkbenchViewModelTest::forwardsSessionAndRuntimeStateNotificationsSeparately()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;
    QSignalSpy sessionSpy(&viewModel, &WorkbenchViewModel::currentSessionChanged);
    QSignalSpy statusSpy(&viewModel, &WorkbenchViewModel::sessionStatusChanged);
    QSignalSpy publishSpy(&viewModel, &WorkbenchViewModel::publishStatusChanged);
    QSignalSpy streamSpy(&viewModel, &WorkbenchViewModel::messageStreamChanged);
    QSignalSpy availabilitySpy(viewModel.publisher(), &PublishComposerViewModel::canPublishChanged);

    emit fixture.sessionService.currentSessionChanged();

    QCOMPARE(sessionSpy.size(), 1);
    QCOMPARE(statusSpy.size(), 1);
    QCOMPARE(publishSpy.size(), 1);
    QCOMPARE(streamSpy.size(), 1);
    QCOMPARE(availabilitySpy.size(), 1);

    emit fixture.mqttService.sessionStateChanged();

    QCOMPARE(sessionSpy.size(), 1);
    QCOMPARE(statusSpy.size(), 2);
    QCOMPARE(publishSpy.size(), 2);
    QCOMPARE(streamSpy.size(), 1);
    QCOMPARE(availabilitySpy.size(), 2);
}

void WorkbenchViewModelTest::forwardsMessageBatchNotifications()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;
    QSignalSpy appendSpy(&viewModel, &WorkbenchViewModel::messageStreamRowsAppended);

    fixture.filteredMessagesModel.setSelectedTopics({QStringLiteral("visible/#")});
    emit fixture.eventHistoryService.messageRowsAppended(QVector<EventRow> {
        EventRow {
            .kind = QStringLiteral("message"),
            .topic = QStringLiteral("visible/one"),
            .direction = QStringLiteral("incoming"),
        },
        EventRow {
            .kind = QStringLiteral("message"),
            .topic = QStringLiteral("hidden/one"),
            .direction = QStringLiteral("incoming"),
        },
    });

    QCOMPARE(appendSpy.size(), 1);
    QCOMPARE(appendSpy.first().at(0).toInt(), 1);

    emit fixture.eventHistoryService.messageRowsAppended(QVector<EventRow> {
        EventRow {
            .kind = QStringLiteral("message"),
            .topic = QStringLiteral("hidden/two"),
            .direction = QStringLiteral("incoming"),
        },
    });
    QCOMPARE(appendSpy.size(), 1);
}

void WorkbenchViewModelTest::exposesTotalMessageCount()
{
    SessionState session;
    session.runtime.totalMessageCount = 1201;
    WorkbenchFixture fixture;
    fixture.sessionService.sessions().append(session);
    fixture.sessionService.setCurrentSessionIndex(0);
    WorkbenchViewModel &viewModel = fixture.viewModel;
    QSignalSpy totalSpy(&viewModel, &WorkbenchViewModel::totalMessageCountChanged);
    QSignalSpy displayTotalSpy(
        &viewModel,
        &WorkbenchViewModel::displayTotalMessageCountChanged);

    QCOMPARE(viewModel.totalMessageCount(), 1201);
    QCOMPARE(viewModel.displayTotalMessageCount(), 1201);

    fixture.sessionService.currentSession()->runtime.totalMessageCount = 1202;
    emit fixture.eventHistoryService.totalMessageCountChanged();
    QCOMPARE(totalSpy.count(), 1);
    QCOMPARE(viewModel.totalMessageCount(), 1202);
    QCOMPARE(displayTotalSpy.count(), 0);
    QTRY_COMPARE(displayTotalSpy.count(), 1);
    QCOMPARE(viewModel.displayTotalMessageCount(), 1202);
}

void WorkbenchViewModelTest::coalescesDisplayTotalMessageCountUpdates()
{
    SessionState session;
    WorkbenchFixture fixture;
    fixture.sessionService.sessions().append(session);
    fixture.sessionService.setCurrentSessionIndex(0);
    WorkbenchViewModel &viewModel = fixture.viewModel;
    QSignalSpy exactTotalSpy(&viewModel, &WorkbenchViewModel::totalMessageCountChanged);
    QSignalSpy displayTotalSpy(
        &viewModel,
        &WorkbenchViewModel::displayTotalMessageCountChanged);

    for (qint64 count = 1; count <= 100; ++count) {
        fixture.sessionService.currentSession()->runtime.totalMessageCount = count;
        emit fixture.eventHistoryService.totalMessageCountChanged();
    }

    QCOMPARE(exactTotalSpy.count(), 100);
    QCOMPARE(displayTotalSpy.count(), 0);
    QCOMPARE(viewModel.totalMessageCount(), 100);
    QCOMPARE(viewModel.displayTotalMessageCount(), 0);
    QTRY_COMPARE(displayTotalSpy.count(), 1);
    QCOMPARE(viewModel.displayTotalMessageCount(), 100);

    SessionState otherSession;
    otherSession.runtime.totalMessageCount = 7;
    fixture.sessionService.sessions().append(otherSession);
    fixture.sessionService.setCurrentSessionIndex(1);
    QCOMPARE(displayTotalSpy.count(), 2);
    QCOMPARE(viewModel.displayTotalMessageCount(), 7);

    fixture.sessionService.currentSession()->runtime.totalMessageCount = 0;
    emit fixture.eventHistoryService.messageStreamChanged();
    QCOMPARE(displayTotalSpy.count(), 3);
    QCOMPARE(viewModel.displayTotalMessageCount(), 0);
}

void WorkbenchViewModelTest::exposesConnectionTimingAndAggregateRates()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    SessionState session;
    session.connectTimeoutSeconds = 8;
    session.runtime.connectedAtMs = nowMs - 5000;
    session.runtime.connectionStartedAtMs = nowMs - 1000;
    session.runtime.recentReceivedTraffic.add(nowMs - 100, 1024);
    session.runtime.recentReceivedTraffic.add(nowMs - 200, 2048);
    session.runtime.recentReceivedTraffic.add(nowMs - 300, 0);
    session.runtime.recentReceivedTraffic.add(nowMs - 1500, 8192);
    session.runtime.recentPublishedTraffic.add(nowMs - 100, 512);
    session.runtime.recentPublishedTraffic.add(nowMs - 200, 0);
    SubscriptionEntry first;
    first.topic = QStringLiteral("devices/one");
    first.recentMessages.add(nowMs - 100);
    first.recentMessages.add(nowMs - 200);
    SubscriptionEntry second;
    second.topic = QStringLiteral("devices/two");
    second.recentMessages.add(nowMs - 300);
    session.subscriptions = {first, second};

    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;
    QSignalSpy trafficSpy(&viewModel, &WorkbenchViewModel::trafficRatesChanged);
    fixture.sessionService.sessions().append(session);
    fixture.sessionService.setCurrentSessionIndex(0);

    const QVariantMap status = viewModel.sessionStatus();
    QCOMPARE(status.value(QStringLiteral("connectedAtMs")).toLongLong(), nowMs - 5000);
    QCOMPARE(status.value(QStringLiteral("connectionStartedAtMs")).toLongLong(), nowMs - 1000);
    QCOMPARE(status.value(QStringLiteral("connectTimeoutSeconds")).toInt(), 8);
    QCOMPARE(viewModel.currentIncomingMessageRate(), 3.0);
    QCOMPARE(viewModel.currentOutgoingMessageRate(), 2.0);
    QCOMPARE(viewModel.currentIncomingByteRate(), qint64(3072));
    QCOMPARE(viewModel.currentOutgoingByteRate(), qint64(512));
    QCOMPARE(viewModel.incomingByteRate(), qint64(3072));
    QCOMPARE(viewModel.outgoingByteRate(), qint64(512));
    QCOMPARE(trafficSpy.count(), 1);

    emit fixture.sessionService.currentSessionChanged();
    QCOMPARE(trafficSpy.count(), 1);
}

void WorkbenchViewModelTest::ownsSubscriptionFilterState()
{
    WorkbenchFixture fixture;
    SubscriptionFilterModel &filteredSubscriptions = fixture.filteredSubscriptionsModel;
    WorkbenchViewModel &viewModel = fixture.viewModel;
    QSignalSpy textSpy(&filteredSubscriptions, &SubscriptionFilterModel::filterTextChanged);
    QSignalSpy modeSpy(&filteredSubscriptions, &SubscriptionFilterModel::filterModeChanged);
    QSignalSpy indexSpy(&filteredSubscriptions, &SubscriptionFilterModel::filterModeIndexChanged);
    QSignalSpy filterSpy(&filteredSubscriptions, &SubscriptionFilterModel::filterChanged);

    QCOMPARE(viewModel.filteredSubscriptions()->filterText(), QString());
    QCOMPARE(viewModel.filteredSubscriptions()->filterMode(), QStringLiteral("all"));
    QCOMPARE(viewModel.filteredSubscriptions()->filterModeIndex(), 0);
    QVERIFY(!viewModel.filteredSubscriptions()->hasFilter());

    viewModel.filteredSubscriptions()->setFilterText(QStringLiteral("  devices/temp  "));
    QCOMPARE(viewModel.filteredSubscriptions()->filterText(), QStringLiteral("devices/temp"));
    QVERIFY(viewModel.filteredSubscriptions()->hasFilter());
    QCOMPARE(textSpy.size(), 1);
    QCOMPARE(filterSpy.size(), 1);

    viewModel.filteredSubscriptions()->setFilterModeIndex(2);
    QCOMPARE(viewModel.filteredSubscriptions()->filterMode(), QStringLiteral("paused"));
    QCOMPARE(viewModel.filteredSubscriptions()->filterModeIndex(), 2);
    QCOMPARE(modeSpy.size(), 1);
    QCOMPARE(indexSpy.size(), 1);

    viewModel.filteredSubscriptions()->setFilterMode(QStringLiteral("invalid"));
    QCOMPARE(viewModel.filteredSubscriptions()->filterMode(), QStringLiteral("all"));
    QCOMPARE(viewModel.filteredSubscriptions()->filterModeIndex(), 0);
    QCOMPARE(modeSpy.size(), 2);
    QCOMPARE(indexSpy.size(), 2);

    viewModel.filteredSubscriptions()->setFilterText(QString());
    QVERIFY(!viewModel.filteredSubscriptions()->hasFilter());
    QCOMPARE(filterSpy.size(), 2);
}

void WorkbenchViewModelTest::ownsPendingSubscriptionDeleteState()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;
    QSignalSpy pendingSpy(&viewModel, &WorkbenchViewModel::pendingSubscriptionDeleteChanged);
    QSignalSpy requestSpy(&viewModel, &WorkbenchViewModel::subscriptionDeleteRequested);

    viewModel.requestSubscriptionDelete(QStringLiteral("devices/temp"), QStringLiteral("Temperature"));

    QCOMPARE(viewModel.pendingSubscriptionDeleteTopic(), QStringLiteral("devices/temp"));
    QCOMPARE(viewModel.pendingSubscriptionDeleteDisplayName(), QStringLiteral("Temperature"));
    QCOMPARE(pendingSpy.size(), 1);
    QCOMPARE(requestSpy.size(), 1);

    QVERIFY(viewModel.confirmPendingSubscriptionDelete());
    QCOMPARE(viewModel.pendingSubscriptionDeleteTopic(), QString());
    QCOMPARE(viewModel.pendingSubscriptionDeleteDisplayName(), QString());
    QCOMPARE(pendingSpy.size(), 2);

    viewModel.requestSubscriptionDelete(QStringLiteral("devices/humidity"), QStringLiteral("Humidity"));
    viewModel.cancelPendingSubscriptionDelete();
    QCOMPARE(viewModel.pendingSubscriptionDeleteTopic(), QString());
    QCOMPARE(viewModel.pendingSubscriptionDeleteDisplayName(), QString());
    QCOMPARE(pendingSpy.size(), 4);
}

void WorkbenchViewModelTest::ownsMessageFilterState()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;

    viewModel.setMessageTopicFilter(QStringLiteral("sensors/+/temperature"));
    QCOMPARE(
        viewModel.filteredMessages()->selectedTopics(),
        QStringList {QStringLiteral("sensors/+/temperature")});

    viewModel.addMessageTopicFilter(QStringLiteral("home/light/set"));
    viewModel.addMessageTopicFilter(QStringLiteral("home/light/set"));
    QCOMPARE(viewModel.filteredMessages()->selectedTopics().size(), 2);

    viewModel.setMessageSearchText(QStringLiteral("temperature"));
    QCOMPARE(viewModel.filteredMessages()->filterText(), QStringLiteral("temperature"));

    viewModel.clearMessageFilters();
    QVERIFY(viewModel.filteredMessages()->selectedTopics().isEmpty());
    QVERIFY(viewModel.filteredMessages()->filterText().isEmpty());
    QCOMPARE(viewModel.filteredMessages()->direction(), QStringLiteral("all"));
}

void WorkbenchViewModelTest::exposesUnfilteredSubscriptionsAndSelectedTopicState()
{
    SessionState session;
    SubscriptionEntry power;
    power.topic = QStringLiteral("sensors/+/power");
    power.alias = QStringLiteral("Power");
    power.paused = true;
    session.subscriptions.append(power);

    SubscriptionEntry light;
    light.topic = QStringLiteral("home/light/set");
    light.alias = QStringLiteral("Light");
    session.subscriptions.append(light);

    WorkbenchFixture fixture;
    SubscriptionListModel subscriptions;
    subscriptions.setSubscriptions(
        session.id,
        session.subscriptions,
        &fixture.processorLibrary);
    fixture.filteredSubscriptionsModel.setSourceModel(&subscriptions);
    fixture.filteredSubscriptionsModel.setFilterText(QStringLiteral("Light"));
    fixture.messageFilterSubscriptionsModel.setSourceModel(&subscriptions);
    WorkbenchViewModel &viewModel = fixture.viewModel;

    QCOMPARE(viewModel.messageFilterSubscriptions()->count(), 2);
    QCOMPARE(viewModel.filteredSubscriptions()->count(), 1);

    viewModel.setMessageTopicFilter(QStringLiteral("sensors/+/power"));
    const QVariantMap oneTopic = viewModel.messageTopicFilterState();
    QCOMPARE(oneTopic.value(QStringLiteral("selectedCount")).toInt(), 1);
    QCOMPARE(oneTopic.value(QStringLiteral("pausedCount")).toInt(), 1);
    QCOMPARE(oneTopic.value(QStringLiteral("singleTopicLabel")).toString(), QStringLiteral("Power"));

    viewModel.addMessageTopicFilter(QStringLiteral("home/light/set"));
    const QVariantMap twoTopics = viewModel.messageTopicFilterState();
    QCOMPARE(twoTopics.value(QStringLiteral("selectedCount")).toInt(), 2);
    QCOMPARE(twoTopics.value(QStringLiteral("pausedCount")).toInt(), 1);
    QVERIFY(twoTopics.value(QStringLiteral("singleTopicLabel")).toString().isEmpty());

    QSignalSpy stateSpy(&viewModel, &WorkbenchViewModel::messageTopicFilterStateChanged);
    session.subscriptions[0].paused = false;
    subscriptions.setSubscriptions(
        session.id,
        session.subscriptions,
        &fixture.processorLibrary);

    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(
        viewModel.messageTopicFilterState().value(QStringLiteral("pausedCount")).toInt(),
        0);
}

void WorkbenchViewModelTest::handlesIntentCommandsWithoutCurrentSession()
{
    WorkbenchFixture fixture;
    WorkbenchViewModel &viewModel = fixture.viewModel;

    viewModel.toggleCurrentSessionConnection();
    fixture.sessionService.setCurrentOutputPaused(true);
    fixture.subscriptionService.setCurrentSubscriptionPaused(QStringLiteral("devices/temp"), true);
    viewModel.copyMessageTopic(QStringLiteral("devices/temp"));
    viewModel.copyMessagePayload(QStringLiteral("0"), QStringLiteral("raw"), QStringLiteral("decoded"), 0);
    fixture.eventHistoryService.clearCurrentMessages();
    QCOMPARE(fixture.eventHistoryService.loadOlderCurrentSessionMessages(), 0);

    QVERIFY(!viewModel.publisher()->canPublish());
}

QTEST_MAIN(WorkbenchViewModelTest)

#include "test_workbenchviewmodel.moc"
