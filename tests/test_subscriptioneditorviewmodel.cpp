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
    void opensForCreateWithInitialTopic();
    void selectsDefaultColorFromPaletteForEachCreate();
    void preservesExplicitColorAndNoColorWhenEditing();
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

void SubscriptionEditorViewModelTest::opensForCreateWithInitialTopic()
{
    SubscriptionEditorViewModel editor;
    editor.openForCreate(QStringLiteral("sensors/room/#"));

    QVERIFY(!editor.editMode());
    QCOMPARE(editor.topic(), QStringLiteral("sensors/room/#"));
    QVERIFY(editor.canSubmit());
}

void SubscriptionEditorViewModelTest::selectsDefaultColorFromPaletteForEachCreate()
{
    SubscriptionEditorViewModel editor;
    for (int attempt = 0; attempt < 32; ++attempt) {
        editor.setColor(QStringLiteral("#123456"));
        editor.openForCreate(QStringLiteral("sensors/room/#"));
        const QString color = editor.color();
        QVERIFY(!color.isEmpty());
        QVERIFY(editor.colorOptions().contains(color));
        editor.setAlias(QStringLiteral("Room sensors"));
        editor.setTopic(QStringLiteral("sensors/other/#"));
        QCOMPARE(editor.color(), color);
        QCOMPARE(editor.submission().color, color);
    }

    editor.setColor({});
    QVERIFY(editor.submission().color.isEmpty());
}

void SubscriptionEditorViewModelTest::preservesExplicitColorAndNoColorWhenEditing()
{
    SubscriptionEditorViewModel editor;
    SubscriptionEntry subscription;
    subscription.topic = QStringLiteral("sensors/room/#");
    subscription.color = QStringLiteral("#123456");
    editor.openForCreate();
    editor.openForEdit(subscription);
    QCOMPARE(editor.color(), subscription.color);
    QCOMPARE(editor.submission().color, subscription.color);

    subscription.color.clear();
    editor.openForEdit(subscription);
    QVERIFY(editor.color().isEmpty());
    QVERIFY(editor.submission().color.isEmpty());
}

void SubscriptionEditorViewModelTest::opensForEditWithExistingProcessorBinding()
{
    SubscriptionEditorViewModel editor;
    editor.setProcessorOptions(processorOptions());
    SubscriptionEntry subscription;
    subscription.topic = QStringLiteral("sensors/+/temp");
    subscription.alias = QStringLiteral("Temperature");
    subscription.requestedQos = 2;
    subscription.format = 1;
    subscription.processor.processorId = QStringLiteral("processor-1");
    subscription.processor.parameters.insert(QStringLiteral("gain"), 4);
    subscription.color = QStringLiteral("#34C759");
    editor.openForEdit(subscription);

    QVERIFY(editor.editMode());
    QCOMPARE(editor.editTopic(), QStringLiteral("sensors/+/temp"));
    QCOMPARE(editor.topic(), QStringLiteral("sensors/+/temp"));
    QCOMPARE(editor.alias(), QStringLiteral("Temperature"));
    QCOMPARE(editor.qos(), 2);
    QCOMPARE(editor.format(), 1);
    QCOMPARE(editor.processorId(), QStringLiteral("processor-1"));
    QVERIFY(editor.processorBindingDetail().isEmpty());
    QCOMPARE(editor.submission().processor.parameters.value(QStringLiteral("gain")).toInteger(), 4);
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
    editor.setNoLocal(true);
    editor.setSubscriptionIdentifierText(QStringLiteral("42"));
    editor.setUserPropertiesText(QStringLiteral("scope=temperature"));

    QVERIFY(editor.canSubmit());
    const SubscriptionEditorSubmission submission = editor.submission();
    QCOMPARE(submission.topic, QStringLiteral("sensors/+/temp"));
    QCOMPARE(submission.processor.processorId, QStringLiteral("processor-1"));
    QVERIFY(submission.options.noLocal);
    QCOMPARE(submission.options.subscriptionIdentifier, quint32(42));
    QCOMPARE(submission.options.userProperties.size(), 1);

    editor.setSubscriptionIdentifierText(QStringLiteral("268435456"));
    QVERIFY(!editor.canSubmit());
    editor.setSubscriptionIdentifierText(QStringLiteral("42"));

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
    QCOMPARE(editor.submission().topics, QStringList({
        QStringLiteral("sensors/one"),
        QStringLiteral("sensors/two"),
        QStringLiteral("sensors/three"),
    }));

    editor.setTopic(QStringLiteral(" , \n "));
    QVERIFY(!editor.canSubmit());
}

QTEST_MAIN(SubscriptionEditorViewModelTest)

#include "test_subscriptioneditorviewmodel.moc"
