#include "viewmodels/drafteditorviewmodel.h"

#include <QtTest/QtTest>

class DraftEditorViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void tracksUnsavedState();
    void duplicatesAsIndependentDraft();
    void supportsQosTwo();
};

void DraftEditorViewModelTest::tracksUnsavedState()
{
    DraftEditorViewModel editor;
    editor.newDraft();
    QVERIFY(!editor.hasUnsavedChanges());
    QVERIFY(!editor.canSave());

    editor.setName(QStringLiteral("Heartbeat"));
    editor.setPayload(QStringLiteral("{}"));
    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(editor.canSave());

    editor.loadDraft({
        {QStringLiteral("id"), QStringLiteral("draft-1")},
        {QStringLiteral("name"), QStringLiteral("Heartbeat")},
        {QStringLiteral("payload"), QStringLiteral("{}")},
        {QStringLiteral("format"), 1},
        {QStringLiteral("qos"), 0},
        {QStringLiteral("retain"), false},
    });
    QVERIFY(!editor.hasUnsavedChanges());
    QVERIFY(!editor.canSave());

    editor.setRetain(true);
    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(editor.canSave());
}

void DraftEditorViewModelTest::duplicatesAsIndependentDraft()
{
    DraftEditorViewModel editor;
    const QVariantMap row {
        {QStringLiteral("id"), QStringLiteral("draft-1")},
        {QStringLiteral("name"), QStringLiteral("Reset")},
        {QStringLiteral("defaultTopic"), QStringLiteral("devices/reset")},
        {QStringLiteral("payload"), QStringLiteral("now")},
        {QStringLiteral("format"), 0},
        {QStringLiteral("qos"), 1},
        {QStringLiteral("retain"), true},
    };
    editor.duplicateDraft(row, QStringLiteral("Reset Copy"));

    QVERIFY(editor.currentDraftId().isEmpty());
    QCOMPARE(editor.name(), QStringLiteral("Reset Copy"));
    QCOMPARE(editor.defaultTopic(), QStringLiteral("devices/reset"));
    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(editor.canSave());
}

void DraftEditorViewModelTest::supportsQosTwo()
{
    DraftEditorViewModel editor;
    editor.newDraft();
    editor.setQos(2);
    QCOMPARE(editor.qos(), 2);

    editor.setQos(3);
    QCOMPARE(editor.qos(), 2);
}

QTEST_MAIN(DraftEditorViewModelTest)

#include "test_drafteditorviewmodel.moc"
