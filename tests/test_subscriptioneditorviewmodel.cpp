#include "viewmodels/subscriptioneditorviewmodel.h"

#include <QtTest/QtTest>

namespace {

QVariantList processorOptions()
{
    return {
        QVariantMap {
            {QStringLiteral("id"), QStringLiteral("processor-1")},
            {QStringLiteral("name"), QStringLiteral("Parser")},
            {QStringLiteral("readinessState"), QStringLiteral("ready")},
        },
    };
}

} // namespace

class SubscriptionEditorViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void opensForCreateWithDefaults();
    void opensForEditWithExistingProcessorBinding();
    void preservesUnavailableBindingWhenOptionsRefresh();
    void validatesAndCollectsCurrentSubmission();
    void parsesAndDeduplicatesBatchTopics();
};

void SubscriptionEditorViewModelTest::opensForCreateWithDefaults()
{
    SubscriptionEditorViewModel editor;
    editor.openForCreate();

    QVERIFY(!editor.editMode());
    QVERIFY(editor.editTopic().isEmpty());
    QVERIFY(editor.topic().isEmpty());
    QVERIFY(editor.alias().isEmpty());
    QCOMPARE(editor.qos(), 0);
    QCOMPARE(editor.format(), 0);
    QVERIFY(editor.processorId().isEmpty());
    QVERIFY(!editor.canSubmit());
}

void SubscriptionEditorViewModelTest::opensForEditWithExistingProcessorBinding()
{
    SubscriptionEditorViewModel editor;
    editor.setProcessorOptions(processorOptions());
    editor.openForEdit({
        {QStringLiteral("topic"), QStringLiteral("sensors/+/temp")},
        {QStringLiteral("alias"), QStringLiteral("Temperature")},
        {QStringLiteral("requestedQos"), 2},
        {QStringLiteral("format"), 1},
        {QStringLiteral("processorId"), QStringLiteral("processor-1")},
        {QStringLiteral("processorParametersCborBase64"), QStringLiteral("oWRnYWluBA==")},
        {QStringLiteral("color"), QStringLiteral("#34C759")},
    });

    QVERIFY(editor.editMode());
    QCOMPARE(editor.editTopic(), QStringLiteral("sensors/+/temp"));
    QCOMPARE(editor.topic(), QStringLiteral("sensors/+/temp"));
    QCOMPARE(editor.alias(), QStringLiteral("Temperature"));
    QCOMPARE(editor.qos(), 2);
    QCOMPARE(editor.format(), 1);
    QCOMPARE(editor.processorId(), QStringLiteral("processor-1"));
    QVERIFY(editor.processorBindingDetail().isEmpty());
}

void SubscriptionEditorViewModelTest::preservesUnavailableBindingWhenOptionsRefresh()
{
    SubscriptionEditorViewModel editor;
    editor.setProcessorOptions(processorOptions());
    editor.setProcessorId(QStringLiteral("missing-processor"));
    editor.setProcessorOptions(processorOptions());

    QCOMPARE(editor.processorOptionIds().last(), QStringLiteral("missing-processor"));
    QVERIFY(editor.processorOptionNames().last().contains(QStringLiteral("Unavailable")));
    QVERIFY(editor.processorBindingDetail().contains(QStringLiteral("preserved")));
}

void SubscriptionEditorViewModelTest::validatesAndCollectsCurrentSubmission()
{
    SubscriptionEditorViewModel editor;
    editor.setProcessorOptions(processorOptions());
    editor.openForCreate();
    editor.setTopic(QStringLiteral(" sensors/+/temp "));
    editor.setAlias(QStringLiteral("Temperature"));
    editor.setQos(2);
    editor.setFormat(2);
    editor.setProcessorId(QStringLiteral("processor-1"));

    QVERIFY(editor.canSubmit());
    const QVariantMap submission = editor.submission();
    QCOMPARE(submission.value(QStringLiteral("topic")).toString(), QStringLiteral("sensors/+/temp"));
    QCOMPARE(submission.value(QStringLiteral("processorId")).toString(), QStringLiteral("processor-1"));

    editor.setQos(3);
    QCOMPARE(editor.qos(), 2);
}

void SubscriptionEditorViewModelTest::parsesAndDeduplicatesBatchTopics()
{
    SubscriptionEditorViewModel editor;
    editor.openForCreate();
    editor.setTopic(QStringLiteral(
        " sensors/one, sensors/two\n"
        "sensors/one\r\n  sensors/three "));

    QVERIFY(editor.canSubmit());
    QCOMPARE(
        editor.submission().value(QStringLiteral("topics")).toStringList(),
        QStringList({
            QStringLiteral("sensors/one"),
            QStringLiteral("sensors/two"),
            QStringLiteral("sensors/three"),
        }));

    editor.setTopic(QStringLiteral(" , \n "));
    QVERIFY(!editor.canSubmit());
}

QTEST_MAIN(SubscriptionEditorViewModelTest)

#include "test_subscriptioneditorviewmodel.moc"
