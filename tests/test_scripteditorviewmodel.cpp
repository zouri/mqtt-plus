#include "viewmodels/scripteditorviewmodel.h"

#include <QtTest/QtTest>

class ScriptEditorViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void newScriptStartsUnsaved();
    void tracksUnsavedChanges();
    void validatesLuaParseShape();
    void marksSavedState();
};

void ScriptEditorViewModelTest::newScriptStartsUnsaved()
{
    ScriptEditorViewModel editor;

    editor.newScript();

    QVERIFY(editor.currentScriptId().isEmpty());
    QCOMPARE(editor.name(), QStringLiteral("New Lua Script"));
    QVERIFY(editor.canSave());
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(editor.validationStatus(), QStringLiteral("Unsaved"));
    QVERIFY(editor.code().contains(QStringLiteral("function constants()")));
    QVERIFY(editor.code().contains(QStringLiteral("function parse(ctx, const)")));
}

void ScriptEditorViewModelTest::tracksUnsavedChanges()
{
    ScriptEditorViewModel editor;
    editor.loadScript(QVariantMap {
        {QStringLiteral("id"), QStringLiteral("script-1")},
        {QStringLiteral("name"), QStringLiteral("Parser")},
        {QStringLiteral("description"), QStringLiteral("Decode payload")},
        {QStringLiteral("code"), QStringLiteral("function parse(ctx)\n    return ctx.decoded\nend\n")},
    });

    QVERIFY(!editor.hasUnsavedChanges());

    editor.setCode(QStringLiteral("function parse(ctx)\n    return ctx.topic\nend\n"));

    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(editor.canSave());
}

void ScriptEditorViewModelTest::validatesLuaParseShape()
{
    ScriptEditorViewModel editor;
    editor.setCode(QStringLiteral("return 1"));

    QVERIFY(!editor.validateStructure());
    QCOMPARE(editor.validationStatus(), QStringLiteral("Structure invalid: define function parse(ctx) ... end"));

    editor.setCode(QStringLiteral("function parse(ctx)\n    return ctx.decoded\nend\n"));

    QVERIFY(editor.validateStructure());
    QCOMPARE(editor.validationStatus(), QStringLiteral("Structure valid"));
}

void ScriptEditorViewModelTest::marksSavedState()
{
    ScriptEditorViewModel editor;
    editor.newScript();
    editor.setName(QStringLiteral("Decoder"));
    editor.setDescription(QStringLiteral("Decode binary payloads"));
    editor.markSaved(QStringLiteral("script-1"));

    QCOMPARE(editor.currentScriptId(), QStringLiteral("script-1"));
    QVERIFY(!editor.hasUnsavedChanges());
    QVERIFY(!editor.canSave());
    QVERIFY(editor.validationOk());
    QCOMPARE(editor.validationStatus(), QStringLiteral("Saved"));
}

QTEST_MAIN(ScriptEditorViewModelTest)

#include "test_scripteditorviewmodel.moc"
