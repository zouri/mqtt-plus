#include "models/scriptlibrarymodel.h"
#include "viewmodels/scriptscoreport.h"
#include "viewmodels/scriptsviewmodel.h"

#include <QtTest/QtTest>

class FakeScriptsCore final : public ScriptsCorePort
{
public:
    void bindScriptsSignals(QObject *, const ScriptsCoreSignalHandlers &newHandlers) override
    {
        handlers = newHandlers;
    }

    ScriptLibraryModel *scripts() override
    {
        return &model;
    }

    QString upsertScript(
        const QString &id,
        const QString &name,
        const QString &description,
        const QString &code) override
    {
        ++upsertCalls;
        lastId = id;
        lastName = name;
        lastDescription = description;
        lastCode = code;
        return savedId;
    }

    ScriptLibraryModel model;
    ScriptsCoreSignalHandlers handlers;
    QString savedId = QStringLiteral("saved-script");
    QString lastId;
    QString lastName;
    QString lastDescription;
    QString lastCode;
    int upsertCalls = 0;
};

class ScriptsViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void matchesScriptFilter();
    void ownsEditorWorkflowCommands();
    void delegatesSaveToCorePort();
    void forwardsScriptLibrarySignal();
    void readsScriptListThroughCorePort();
};

void ScriptsViewModelTest::matchesScriptFilter()
{
    QVERIFY(ScriptsViewModel::scriptMatchesFilter(
        QStringLiteral("Decoder"),
        QStringLiteral("Binary payload"),
        QStringLiteral("function parse(ctx)\nend\n"),
        QStringLiteral("decode")));
    QVERIFY(ScriptsViewModel::scriptMatchesFilter(
        QStringLiteral("Decoder"),
        QStringLiteral("Binary payload"),
        QStringLiteral("function parse(ctx)\nend\n"),
        QStringLiteral(" parse ")));
    QVERIFY(!ScriptsViewModel::scriptMatchesFilter(
        QStringLiteral("Decoder"),
        QStringLiteral("Binary payload"),
        QStringLiteral("function parse(ctx)\nend\n"),
        QStringLiteral("temperature")));
}

void ScriptsViewModelTest::ownsEditorWorkflowCommands()
{
    ScriptsViewModel viewModel;

    viewModel.newScript();

    QCOMPARE(viewModel.editor()->name(), QStringLiteral("New Lua Script"));
    QVERIFY(viewModel.editor()->canSave());

    viewModel.editor()->setCode(QStringLiteral("return 1"));
    QVERIFY(!viewModel.validateEditorStructure());
    QCOMPARE(viewModel.editor()->validationStatus(), QStringLiteral("Structure invalid: define function parse(ctx) ... end"));
}

void ScriptsViewModelTest::delegatesSaveToCorePort()
{
    FakeScriptsCore core;
    ScriptsViewModel viewModel(&core);

    viewModel.newScript();
    viewModel.editor()->setName(QStringLiteral("Decoder"));
    viewModel.editor()->setDescription(QStringLiteral("Binary payload"));
    viewModel.editor()->setCode(QStringLiteral("function parse(ctx)\n    return ctx.payload\nend"));

    QVERIFY(viewModel.saveEditor());
    QCOMPARE(core.upsertCalls, 1);
    QCOMPARE(core.lastName, QStringLiteral("Decoder"));
    QCOMPARE(core.lastDescription, QStringLiteral("Binary payload"));
    QCOMPARE(core.lastCode, QStringLiteral("function parse(ctx)\n    return ctx.payload\nend"));
    QCOMPARE(viewModel.editor()->currentScriptId(), QStringLiteral("saved-script"));
    QVERIFY(!viewModel.editor()->hasUnsavedChanges());
}

void ScriptsViewModelTest::forwardsScriptLibrarySignal()
{
    FakeScriptsCore core;
    ScriptsViewModel viewModel(&core);
    QSignalSpy spy(&viewModel, &ScriptsViewModel::scriptLibraryChanged);

    QVERIFY(core.handlers.scriptLibraryChanged);
    core.handlers.scriptLibraryChanged();
    QCOMPARE(spy.count(), 1);
}

void ScriptsViewModelTest::readsScriptListThroughCorePort()
{
    FakeScriptsCore core;
    core.model.setRows({
        {
            QStringLiteral("decoder"),
            QStringLiteral("Decoder"),
            QStringLiteral("Binary payload"),
            QStringLiteral("function parse(ctx)\nend"),
            {},
            {},
        },
        {
            QStringLiteral("logger"),
            QStringLiteral("Logger"),
            QStringLiteral("Debug event"),
            QStringLiteral("function parse(ctx)\nend"),
            {},
            {},
        },
    });
    ScriptsViewModel viewModel(&core);

    QCOMPARE(viewModel.scripts(), &core.model);
    QCOMPARE(viewModel.visibleScriptCount(QStringLiteral("binary")), 1);
    QVERIFY(viewModel.selectScriptAt(1));
    QCOMPARE(viewModel.editor()->currentScriptId(), QStringLiteral("logger"));
}

QTEST_MAIN(ScriptsViewModelTest)

#include "test_scriptsviewmodel.moc"
