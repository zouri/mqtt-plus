#include "viewmodels/workbenchviewmodel.h"

#include <QtTest/QtTest>

class WorkbenchViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesDefaultPublishDraft();
    void exposesSessionEditor();
    void exposesSubscriptionEditor();
    void updatesPublishDraft();
    void rejectsPublishWithoutConnectedSession();
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

    QCOMPARE(viewModel.publishTopic(), QStringLiteral(" sensors/temp "));
    QCOMPARE(viewModel.publishPayload(), QStringLiteral("{\"value\":23}"));
    QCOMPARE(viewModel.publishFormat(), 2);
    QCOMPARE(viewModel.publishQos(), 1);
    QCOMPARE(viewModel.publishRetain(), true);
    QCOMPARE(topicSpy.size(), 1);
    QCOMPARE(payloadSpy.size(), 1);
    QCOMPARE(formatSpy.size(), 1);
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

QTEST_MAIN(WorkbenchViewModelTest)

#include "test_workbenchviewmodel.moc"
