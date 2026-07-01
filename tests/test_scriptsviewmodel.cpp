#include "domain/script.h"
#include "models/scriptlibrarymodel.h"
#include "viewmodels/scriptsviewmodel.h"

#include <QtTest/QtTest>

#include <functional>
#include <utility>

class FakeScriptsDeps
{
public:
    ScriptsViewModel::Dependencies dependencies()
    {
        return {
            &model,
            [this](QObject *, std::function<void()> handler) {
                scriptLibraryChanged = std::move(handler);
            },
            [this](
                const QString &id,
                const QString &name,
                const QString &description,
                const QString &code) {
                ++upsertCalls;
                lastId = id;
                lastName = name;
                lastDescription = description;
                lastCode = code;
                return savedId;
            },
        };
    }

    ScriptLibraryModel model;
    std::function<void()> scriptLibraryChanged;
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
    void delegatesSaveToDependencies();
    void forwardsScriptLibrarySignal();
    void readsScriptListThroughDependencies();
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

void ScriptsViewModelTest::delegatesSaveToDependencies()
{
    FakeScriptsDeps deps;
    ScriptsViewModel viewModel(deps.dependencies());

    viewModel.newScript();
    viewModel.editor()->setName(QStringLiteral("Decoder"));
    viewModel.editor()->setDescription(QStringLiteral("Binary payload"));
    viewModel.editor()->setCode(QStringLiteral("function parse(ctx)\n    return ctx.payload\nend"));

    QVERIFY(viewModel.saveEditor());
    QCOMPARE(deps.upsertCalls, 1);
    QCOMPARE(deps.lastName, QStringLiteral("Decoder"));
    QCOMPARE(deps.lastDescription, QStringLiteral("Binary payload"));
    QCOMPARE(deps.lastCode, QStringLiteral("function parse(ctx)\n    return ctx.payload\nend"));
    QCOMPARE(viewModel.editor()->currentScriptId(), QStringLiteral("saved-script"));
    QVERIFY(!viewModel.editor()->hasUnsavedChanges());
}

void ScriptsViewModelTest::forwardsScriptLibrarySignal()
{
    FakeScriptsDeps deps;
    ScriptsViewModel viewModel(deps.dependencies());
    QSignalSpy spy(&viewModel, &ScriptsViewModel::scriptLibraryChanged);

    QVERIFY(deps.scriptLibraryChanged);
    deps.scriptLibraryChanged();
    QCOMPARE(spy.count(), 1);
}

void ScriptsViewModelTest::readsScriptListThroughDependencies()
{
    FakeScriptsDeps deps;
    QVector<ScriptEntry> scripts {
        {
            QStringLiteral("decoder"),
            QStringLiteral("Decoder"),
            QStringLiteral("Binary payload"),
            QStringLiteral("function parse(ctx)\nend"),
        },
        {
            QStringLiteral("logger"),
            QStringLiteral("Logger"),
            QStringLiteral("Debug event"),
            QStringLiteral("function parse(ctx)\nend"),
        },
    };
    deps.model.setSource(&scripts);
    ScriptsViewModel viewModel(deps.dependencies());

    QCOMPARE(viewModel.scripts(), &deps.model);
    QCOMPARE(viewModel.visibleScriptCount(QStringLiteral("binary")), 1);
    QVERIFY(viewModel.selectScriptAt(1));
    QCOMPARE(viewModel.editor()->currentScriptId(), QStringLiteral("logger"));
}

QTEST_MAIN(ScriptsViewModelTest)

#include "test_scriptsviewmodel.moc"
