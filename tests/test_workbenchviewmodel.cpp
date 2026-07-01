#include "viewmodels/workbenchviewmodel.h"

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
    void ignoresContextMenusWithoutCore();
    void updatesPublishDraft();
    void rejectsPublishWithoutConnectedSession();
    void ownsSubscriptionFilterState();
    void ownsPendingSubscriptionDeleteState();
    void acceptsIntentCommandsWithoutCore();
};

void WorkbenchViewModelTest::exposesDefaultPublishDraft()
{
    WorkbenchViewModel viewModel;

    QCOMPARE(viewModel.publishTopic(), QString());
    QCOMPARE(viewModel.publishPayload(), QString());
    QCOMPARE(viewModel.publishFormat(), 1);
    QCOMPARE(viewModel.publishQos(), 0);
    QCOMPARE(viewModel.publishRetain(), false);
    QVERIFY(!viewModel.canPublish());
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

void WorkbenchViewModelTest::ignoresContextMenusWithoutCore()
{
    WorkbenchViewModel viewModel;
    QSignalSpy sessionEditSpy(&viewModel, &WorkbenchViewModel::sessionEditRequested);
    QSignalSpy subscriptionEditSpy(&viewModel, &WorkbenchViewModel::subscriptionEditRequested);
    QSignalSpy subscriptionDeleteSpy(&viewModel, &WorkbenchViewModel::subscriptionDeleteRequested);

    viewModel.handleSessionContextMenu(0, QPointF());
    viewModel.handleSubscriptionContextMenu(0, QStringLiteral("devices/+/temp"), QPointF());

    QCOMPARE(sessionEditSpy.size(), 0);
    QCOMPARE(subscriptionEditSpy.size(), 0);
    QCOMPARE(subscriptionDeleteSpy.size(), 0);
}

void WorkbenchViewModelTest::updatesPublishDraft()
{
    WorkbenchViewModel viewModel;
    QSignalSpy topicSpy(&viewModel, &WorkbenchViewModel::publishTopicChanged);
    QSignalSpy payloadSpy(&viewModel, &WorkbenchViewModel::publishPayloadChanged);
    QSignalSpy formatSpy(&viewModel, &WorkbenchViewModel::publishFormatChanged);
    QSignalSpy qosSpy(&viewModel, &WorkbenchViewModel::publishQosChanged);
    QSignalSpy retainSpy(&viewModel, &WorkbenchViewModel::publishRetainChanged);

    viewModel.setPublishTopic(QStringLiteral(" sensors/temp "));
    viewModel.setPublishPayload(QStringLiteral("{\"value\":23}"));
    viewModel.setPublishFormat(2);
    viewModel.setPublishQos(1);
    viewModel.setPublishRetain(true);
    viewModel.useMessageAsPublishDraft(QStringLiteral("devices/humidity"), QStringLiteral("raw"), QStringLiteral("decoded"), 0);

    QCOMPARE(viewModel.publishTopic(), QStringLiteral("devices/humidity"));
    QCOMPARE(viewModel.publishPayload(), QStringLiteral("decoded"));
    QCOMPARE(viewModel.publishFormat(), 0);
    QCOMPARE(viewModel.publishQos(), 1);
    QCOMPARE(viewModel.publishRetain(), true);
    QCOMPARE(topicSpy.size(), 2);
    QCOMPARE(payloadSpy.size(), 2);
    QCOMPARE(formatSpy.size(), 2);
    QCOMPARE(qosSpy.size(), 1);
    QCOMPARE(retainSpy.size(), 1);
}

void WorkbenchViewModelTest::rejectsPublishWithoutConnectedSession()
{
    WorkbenchViewModel viewModel;

    viewModel.setPublishTopic(QStringLiteral("sensors/temp"));
    viewModel.setPublishPayload(QStringLiteral("23"));

    QVERIFY(!viewModel.canPublish());
    QVERIFY(!viewModel.publishDraft());
    QCOMPARE(viewModel.publishTopic(), QStringLiteral("sensors/temp"));
    QCOMPARE(viewModel.publishPayload(), QStringLiteral("23"));
}

void WorkbenchViewModelTest::ownsSubscriptionFilterState()
{
    WorkbenchViewModel viewModel;
    QSignalSpy textSpy(&viewModel, &WorkbenchViewModel::subscriptionFilterTextChanged);
    QSignalSpy modeSpy(&viewModel, &WorkbenchViewModel::subscriptionFilterModeChanged);
    QSignalSpy indexSpy(&viewModel, &WorkbenchViewModel::subscriptionFilterModeIndexChanged);
    QSignalSpy filterSpy(&viewModel, &WorkbenchViewModel::subscriptionFilterChanged);

    QCOMPARE(viewModel.subscriptionFilterText(), QString());
    QCOMPARE(viewModel.subscriptionFilterMode(), QStringLiteral("all"));
    QCOMPARE(viewModel.subscriptionFilterModeIndex(), 0);
    QVERIFY(!viewModel.hasSubscriptionFilter());

    viewModel.setSubscriptionFilterText(QStringLiteral("  devices/temp  "));
    QCOMPARE(viewModel.subscriptionFilterText(), QStringLiteral("devices/temp"));
    QVERIFY(viewModel.hasSubscriptionFilter());
    QCOMPARE(textSpy.size(), 1);
    QCOMPARE(filterSpy.size(), 1);

    viewModel.setSubscriptionFilterModeIndex(2);
    QCOMPARE(viewModel.subscriptionFilterMode(), QStringLiteral("paused"));
    QCOMPARE(viewModel.subscriptionFilterModeIndex(), 2);
    QCOMPARE(modeSpy.size(), 1);
    QCOMPARE(indexSpy.size(), 1);

    viewModel.setSubscriptionFilterMode(QStringLiteral("invalid"));
    QCOMPARE(viewModel.subscriptionFilterMode(), QStringLiteral("all"));
    QCOMPARE(viewModel.subscriptionFilterModeIndex(), 0);
    QCOMPARE(modeSpy.size(), 2);
    QCOMPARE(indexSpy.size(), 2);

    viewModel.setSubscriptionFilterText(QString());
    QVERIFY(!viewModel.hasSubscriptionFilter());
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

void WorkbenchViewModelTest::acceptsIntentCommandsWithoutCore()
{
    WorkbenchViewModel viewModel;

    viewModel.toggleCurrentSessionConnection();
    viewModel.toggleCurrentOutputPaused(false);
    viewModel.toggleCurrentSubscriptionPaused(QStringLiteral("devices/temp"), false);
    viewModel.copyMessageTopic(QStringLiteral("devices/temp"));
    viewModel.copyMessagePayload(QStringLiteral("raw"), QStringLiteral("decoded"));
    viewModel.clearMessages();
    QCOMPARE(viewModel.loadOlderMessages(), 0);

    QVERIFY(!viewModel.canPublish());
}

QTEST_MAIN(WorkbenchViewModelTest)

#include "test_workbenchviewmodel.moc"
