#include "viewmodels/workbenchviewmodel.h"

#include "domain/session.h"
#include "models/subscriptionlistmodel.h"
#include "usecases/eventhistoryservice.h"
#include "usecases/sessionservice.h"

#include <QtTest/QtTest>

class WorkbenchViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesDefaultPublishDraft();
    void exposesSessionEditor();
    void exposesSubscriptionEditor();
    void preparesSubscriptionEditorForCreate();
    void rejectsSubscriptionEditorEditWithoutCore();
    void ignoresSessionCommandsWithoutCore();
    void updatesPublishDraft();
    void rejectsPublishWithoutConnectedSession();
    void forwardsSessionAndRuntimeStateNotificationsSeparately();
    void forwardsMessageBatchNotifications();
    void exposesTotalMessageCountAndForwardsFreeze();
    void ownsSubscriptionFilterState();
    void ownsPendingSubscriptionDeleteState();
    void acceptsIntentCommandsWithoutCore();
    void ownsMessageFilterState();
    void exposesUnfilteredSubscriptionsAndSelectedTopicState();
};

void WorkbenchViewModelTest::exposesDefaultPublishDraft()
{
    WorkbenchViewModel viewModel;

    QVERIFY(viewModel.publisher());
    QCOMPARE(viewModel.publisher()->topic(), QString());
    QCOMPARE(viewModel.publisher()->payload(), QString());
    QCOMPARE(viewModel.publisher()->format(), 1);
    QCOMPARE(viewModel.publisher()->qos(), 0);
    QCOMPARE(viewModel.publisher()->retain(), false);
    QVERIFY(!viewModel.publisher()->canPublish());
}

void WorkbenchViewModelTest::exposesSessionEditor()
{
    WorkbenchViewModel viewModel;

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
    WorkbenchViewModel viewModel;

    QVERIFY(viewModel.subscriptionEditor());
    viewModel.subscriptionEditor()->openForCreate();
    viewModel.subscriptionEditor()->setTopic(QStringLiteral("devices/+/temp"));

    QVERIFY(!viewModel.submitSubscriptionEditor());
    QCOMPARE(viewModel.subscriptionEditor()->topic(), QStringLiteral("devices/+/temp"));
}

void WorkbenchViewModelTest::preparesSubscriptionEditorForCreate()
{
    WorkbenchViewModel viewModel;
    viewModel.subscriptionEditor()->setTopic(QStringLiteral("devices/+/temp"));
    viewModel.subscriptionEditor()->setAlias(QStringLiteral("Temperature"));

    viewModel.openSubscriptionEditorForCreate();

    QVERIFY(!viewModel.subscriptionEditor()->editMode());
    QVERIFY(viewModel.subscriptionEditor()->topic().isEmpty());
    QVERIFY(viewModel.subscriptionEditor()->alias().isEmpty());
}

void WorkbenchViewModelTest::rejectsSubscriptionEditorEditWithoutCore()
{
    WorkbenchViewModel viewModel;

    QVERIFY(!viewModel.openSubscriptionEditorForEdit(0));
    QVERIFY(!viewModel.subscriptionEditor()->editMode());
}

void WorkbenchViewModelTest::ignoresSessionCommandsWithoutCore()
{
    WorkbenchViewModel viewModel;
    QSignalSpy sessionEditSpy(&viewModel, &WorkbenchViewModel::sessionEditRequested);

    viewModel.requestSessionDuplicate(0);
    viewModel.requestSessionDelete(0);

    QCOMPARE(sessionEditSpy.size(), 0);
}

void WorkbenchViewModelTest::updatesPublishDraft()
{
    WorkbenchViewModel viewModel;
    auto *publisher = viewModel.publisher();
    QSignalSpy topicSpy(publisher, &PublishDraftViewModel::topicChanged);
    QSignalSpy payloadSpy(publisher, &PublishDraftViewModel::payloadChanged);
    QSignalSpy formatSpy(publisher, &PublishDraftViewModel::formatChanged);
    QSignalSpy qosSpy(publisher, &PublishDraftViewModel::qosChanged);
    QSignalSpy retainSpy(publisher, &PublishDraftViewModel::retainChanged);

    publisher->setTopic(QStringLiteral(" sensors/temp "));
    publisher->setPayload(QStringLiteral("{\"value\":23}"));
    publisher->setFormat(2);
    publisher->setQos(1);
    publisher->setRetain(true);
    publisher->useMessageAsDraft(QStringLiteral("devices/humidity"), QStringLiteral("raw"), QStringLiteral("decoded"), 0);

    QCOMPARE(publisher->topic(), QStringLiteral("devices/humidity"));
    QCOMPARE(publisher->payload(), QStringLiteral("decoded"));
    QCOMPARE(publisher->format(), 0);
    QCOMPARE(publisher->qos(), 1);
    QCOMPARE(publisher->retain(), true);
    QCOMPARE(topicSpy.size(), 2);
    QCOMPARE(payloadSpy.size(), 2);
    QCOMPARE(formatSpy.size(), 2);
    QCOMPARE(qosSpy.size(), 1);
    QCOMPARE(retainSpy.size(), 1);
}

void WorkbenchViewModelTest::rejectsPublishWithoutConnectedSession()
{
    WorkbenchViewModel viewModel;

    viewModel.publisher()->setTopic(QStringLiteral("sensors/temp"));
    viewModel.publisher()->setPayload(QStringLiteral("23"));

    QVERIFY(!viewModel.publisher()->canPublish());
    QVERIFY(!viewModel.publisher()->publishDraft());
    QCOMPARE(viewModel.publisher()->topic(), QStringLiteral("sensors/temp"));
    QCOMPARE(viewModel.publisher()->payload(), QStringLiteral("23"));
}

void WorkbenchViewModelTest::forwardsSessionAndRuntimeStateNotificationsSeparately()
{
    std::function<void()> notifyCurrentSession;
    std::function<void()> notifySessionRuntimeState;
    WorkbenchViewModel::Dependencies dependencies;
    dependencies.bindCurrentSessionChanged = [&notifyCurrentSession](QObject *, std::function<void()> handler) {
        notifyCurrentSession = std::move(handler);
    };
    dependencies.bindSessionRuntimeStateChanged = [&notifySessionRuntimeState](QObject *, std::function<void()> handler) {
        notifySessionRuntimeState = std::move(handler);
    };
    WorkbenchViewModel viewModel(dependencies);
    QSignalSpy sessionSpy(&viewModel, &WorkbenchViewModel::currentSessionChanged);
    QSignalSpy statusSpy(&viewModel, &WorkbenchViewModel::sessionStatusChanged);
    QSignalSpy publishSpy(&viewModel, &WorkbenchViewModel::publishStatusChanged);

    QVERIFY(notifyCurrentSession);
    QVERIFY(notifySessionRuntimeState);
    notifyCurrentSession();

    QCOMPARE(sessionSpy.size(), 1);
    QCOMPARE(statusSpy.size(), 1);
    QCOMPARE(publishSpy.size(), 1);

    notifySessionRuntimeState();

    QCOMPARE(sessionSpy.size(), 1);
    QCOMPARE(statusSpy.size(), 2);
    QCOMPARE(publishSpy.size(), 2);
}

void WorkbenchViewModelTest::forwardsMessageBatchNotifications()
{
    std::function<void(int)> notifyMessageRows;
    WorkbenchViewModel::Dependencies dependencies;
    dependencies.bindMessageStreamRowsAppended = [&notifyMessageRows](QObject *, std::function<void(int)> handler) {
        notifyMessageRows = std::move(handler);
    };
    WorkbenchViewModel viewModel(dependencies);
    QSignalSpy appendSpy(&viewModel, &WorkbenchViewModel::messageStreamRowsAppended);

    QVERIFY(notifyMessageRows);
    notifyMessageRows(4);

    QCOMPARE(appendSpy.size(), 1);
    QCOMPARE(appendSpy.first().at(0).toInt(), 4);
}

void WorkbenchViewModelTest::exposesTotalMessageCountAndForwardsFreeze()
{
    std::function<void()> notifyTotalMessageCount;
    SessionState session;
    session.runtime.totalMessageCount = 1201;
    SessionService sessions;
    sessions.appendSession(session);
    sessions.setCurrentIndex(0);
    EventHistoryService history;

    WorkbenchViewModel::Dependencies dependencies;
    dependencies.bindTotalMessageCountChanged = [&notifyTotalMessageCount](QObject *, std::function<void()> handler) {
        notifyTotalMessageCount = std::move(handler);
    };
    dependencies.sessionController = &sessions;
    dependencies.eventController = &history;
    WorkbenchViewModel viewModel(dependencies);
    QSignalSpy totalSpy(&viewModel, &WorkbenchViewModel::totalMessageCountChanged);

    QCOMPARE(viewModel.totalMessageCount(), 1201);
    viewModel.setMessageStreamFrozen(true);
    QVERIFY(history.messageStreamFrozen());

    sessions.currentSession()->runtime.totalMessageCount = 1202;
    notifyTotalMessageCount();
    QCOMPARE(totalSpy.count(), 1);
    QCOMPARE(viewModel.totalMessageCount(), 1202);
}

void WorkbenchViewModelTest::ownsSubscriptionFilterState()
{
    SubscriptionFilterModel filteredSubscriptions;
    WorkbenchViewModel::Dependencies dependencies;
    dependencies.filteredSubscriptions = &filteredSubscriptions;
    WorkbenchViewModel viewModel(dependencies);
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
    WorkbenchViewModel viewModel;
    QSignalSpy pendingSpy(&viewModel, &WorkbenchViewModel::pendingSubscriptionDeleteChanged);
    QSignalSpy requestSpy(&viewModel, &WorkbenchViewModel::subscriptionDeleteRequested);

    viewModel.requestSubscriptionDelete(QStringLiteral("devices/temp"), QStringLiteral("Temperature"));

    QCOMPARE(viewModel.pendingSubscriptionDeleteTopic(), QStringLiteral("devices/temp"));
    QCOMPARE(viewModel.pendingSubscriptionDeleteDisplayName(), QStringLiteral("Temperature"));
    QCOMPARE(pendingSpy.size(), 1);
    QCOMPARE(requestSpy.size(), 1);

    QVERIFY(!viewModel.confirmPendingSubscriptionDelete());
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
    MessageFilterModel filteredMessages;
    WorkbenchViewModel::Dependencies dependencies;
    dependencies.filteredMessages = &filteredMessages;
    WorkbenchViewModel viewModel(dependencies);

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

    SubscriptionListModel subscriptions;
    subscriptions.setSource(&session);
    SubscriptionFilterModel filteredSubscriptions;
    filteredSubscriptions.setSourceModel(&subscriptions);
    filteredSubscriptions.setFilterText(QStringLiteral("Light"));
    SubscriptionFilterModel messageFilterSubscriptions;
    messageFilterSubscriptions.setSourceModel(&subscriptions);
    MessageFilterModel filteredMessages;

    WorkbenchViewModel::Dependencies dependencies;
    dependencies.filteredSubscriptions = &filteredSubscriptions;
    dependencies.messageFilterSubscriptions = &messageFilterSubscriptions;
    dependencies.filteredMessages = &filteredMessages;
    WorkbenchViewModel viewModel(dependencies);

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
}

void WorkbenchViewModelTest::acceptsIntentCommandsWithoutCore()
{
    WorkbenchViewModel viewModel;

    viewModel.toggleCurrentSessionConnection();
    viewModel.toggleCurrentOutputPaused(false);
    viewModel.toggleCurrentSubscriptionPaused(QStringLiteral("devices/temp"), false);
    viewModel.copyMessageTopic(QStringLiteral("devices/temp"));
    viewModel.copyMessagePayload(QStringLiteral("0"), QStringLiteral("raw"), QStringLiteral("decoded"), 0);
    viewModel.clearMessages();
    QCOMPARE(viewModel.loadOlderMessages(), 0);

    QVERIFY(!viewModel.publisher()->canPublish());
}

QTEST_MAIN(WorkbenchViewModelTest)

#include "test_workbenchviewmodel.moc"
