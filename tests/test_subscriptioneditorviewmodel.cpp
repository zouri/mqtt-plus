#include "viewmodels/subscriptioneditorviewmodel.h"

#include <QtTest/QtTest>

class SubscriptionEditorViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void opensForCreateWithDefaults();
    void opensForEditWithExistingValues();
    void preservesSelectedScriptWhenOptionsRefresh();
    void validatesAndCollectsSubmission();
};

void SubscriptionEditorViewModelTest::opensForCreateWithDefaults()
{
    SubscriptionEditorViewModel editor;

    editor.openForCreate();

    QVERIFY(!editor.editMode());
    QVERIFY(editor.editTopic().isEmpty());
    QVERIFY(editor.topic().isEmpty());
    QCOMPARE(editor.qos(), 0);
    QCOMPARE(editor.format(), 0);
    QVERIFY(editor.scriptId().isEmpty());
}

void SubscriptionEditorViewModelTest::opensForEditWithExistingValues()
{
    SubscriptionEditorViewModel editor;
    QVariantMap subscription;
    subscription.insert(QStringLiteral("topic"), QStringLiteral("devices/+/temp"));
    subscription.insert(QStringLiteral("alias"), QStringLiteral("Temperature"));
    subscription.insert(QStringLiteral("requestedQos"), 1);
    subscription.insert(QStringLiteral("format"), 2);
    subscription.insert(QStringLiteral("scriptId"), QStringLiteral("script-1"));

    editor.openForEdit(subscription);

    QVERIFY(editor.editMode());
    QCOMPARE(editor.editTopic(), QStringLiteral("devices/+/temp"));
    QCOMPARE(editor.topic(), QStringLiteral("devices/+/temp"));
    QCOMPARE(editor.alias(), QStringLiteral("Temperature"));
    QCOMPARE(editor.qos(), 1);
    QCOMPARE(editor.format(), 2);
    QCOMPARE(editor.scriptId(), QStringLiteral("script-1"));
}

void SubscriptionEditorViewModelTest::preservesSelectedScriptWhenOptionsRefresh()
{
    SubscriptionEditorViewModel editor;
    QVariantList scripts;
    scripts.append(QVariantMap {
        {QStringLiteral("id"), QStringLiteral("script-1")},
        {QStringLiteral("name"), QStringLiteral("Parser")},
    });
    scripts.append(QVariantMap {
        {QStringLiteral("id"), QStringLiteral("script-2")},
        {QStringLiteral("name"), QStringLiteral("Decoder")},
    });

    editor.setScriptOptions(scripts);
    editor.setScriptId(QStringLiteral("script-2"));
    editor.setScriptOptions(scripts);

    QCOMPARE(editor.scriptOptionIds(), QStringList({QString(), QStringLiteral("script-1"), QStringLiteral("script-2")}));
    QCOMPARE(editor.scriptOptionNames(), QStringList({QStringLiteral("None"), QStringLiteral("Parser"), QStringLiteral("Decoder")}));
    QCOMPARE(editor.scriptIndex(), 2);
}

void SubscriptionEditorViewModelTest::validatesAndCollectsSubmission()
{
    SubscriptionEditorViewModel editor;

    editor.openForCreate();
    QVERIFY(!editor.canSubmit());

    editor.setTopic(QStringLiteral(" sensors/+/temp "));
    editor.setAlias(QStringLiteral("Temperature"));
    editor.setQos(1);
    editor.setFormat(2);
    editor.setScriptId(QStringLiteral("script-1"));

    QVERIFY(editor.canSubmit());
    const QVariantMap submission = editor.submission();
    QCOMPARE(submission.value(QStringLiteral("editMode")).toBool(), false);
    QCOMPARE(submission.value(QStringLiteral("editTopic")).toString(), QString());
    QCOMPARE(submission.value(QStringLiteral("topic")).toString(), QStringLiteral("sensors/+/temp"));
    QCOMPARE(submission.value(QStringLiteral("alias")).toString(), QStringLiteral("Temperature"));
    QCOMPARE(submission.value(QStringLiteral("qos")).toInt(), 1);
    QCOMPARE(submission.value(QStringLiteral("format")).toInt(), 2);
    QCOMPARE(submission.value(QStringLiteral("scriptId")).toString(), QStringLiteral("script-1"));
}

QTEST_MAIN(SubscriptionEditorViewModelTest)

#include "test_subscriptioneditorviewmodel.moc"
