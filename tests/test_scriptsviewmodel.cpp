#include "domain/script.h"
#include "models/scriptlibrarymodel.h"
#include "usecases/scriptservice.h"
#include "viewmodels/scriptsviewmodel.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QStandardPaths>

namespace {

QString scriptStoragePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("mqtt_plus/scripts"));
}

bool clearScriptStorage()
{
    QDir directory(scriptStoragePath());
    return !directory.exists() || directory.removeRecursively();
}

} // namespace

class ScriptsViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void matchesScriptFilter();
    void ownsEditorWorkflowCommands();
    void savesThroughScriptService();
    void forwardsScriptLibrarySignal();
    void readsConcreteScriptModel();
};

void ScriptsViewModelTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void ScriptsViewModelTest::init()
{
    QVERIFY2(clearScriptStorage(), qPrintable(QStringLiteral("Cannot clear %1").arg(scriptStoragePath())));
}

void ScriptsViewModelTest::cleanup()
{
    QVERIFY2(clearScriptStorage(), qPrintable(QStringLiteral("Cannot clear %1").arg(scriptStoragePath())));
}

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
    ScriptService service;
    ScriptLibraryModel model;
    ScriptsViewModel viewModel(service, model);

    viewModel.newScript();

    QCOMPARE(viewModel.editor()->name(), QStringLiteral("New Lua Script"));
    QVERIFY(viewModel.editor()->canSave());

    viewModel.editor()->setCode(QStringLiteral("return 1"));
    QVERIFY(!viewModel.validateEditorStructure());
    QCOMPARE(viewModel.editor()->validationStatus(), QStringLiteral("Structure invalid: define function parse(ctx) ... end"));
}

void ScriptsViewModelTest::savesThroughScriptService()
{
    ScriptService service;
    ScriptLibraryModel model;
    ScriptsViewModel viewModel(service, model);
    QSignalSpy librarySpy(&viewModel, &ScriptsViewModel::scriptLibraryChanged);
    QSignalSpy storageSpy(&service, &ScriptService::storageError);

    viewModel.newScript();
    viewModel.editor()->setName(QStringLiteral("Decoder"));
    viewModel.editor()->setDescription(QStringLiteral("Binary payload"));
    viewModel.editor()->setCode(QStringLiteral("function parse(ctx)\n    return ctx.payload\nend"));

    const bool saved = viewModel.saveEditor();
    const QString storageError = storageSpy.isEmpty()
        ? QStringLiteral("ScriptService rejected the save without an error")
        : storageSpy.constLast().at(0).toString();
    QVERIFY2(saved, qPrintable(storageError));
    QCOMPARE(librarySpy.count(), 1);
    QCOMPARE(model.count(), 1);

    const QString savedId = viewModel.editor()->currentScriptId();
    QVERIFY(!savedId.isEmpty());
    const ScriptEntry *savedScript = service.scriptById(savedId);
    QVERIFY(savedScript);
    QCOMPARE(savedScript->name, QStringLiteral("Decoder"));
    QCOMPARE(savedScript->description, QStringLiteral("Binary payload"));
    QCOMPARE(savedScript->code, QStringLiteral("function parse(ctx)\n    return ctx.payload\nend"));
    QVERIFY(!viewModel.editor()->hasUnsavedChanges());
}

void ScriptsViewModelTest::forwardsScriptLibrarySignal()
{
    ScriptService service;
    ScriptLibraryModel model;
    ScriptsViewModel viewModel(service, model);
    QSignalSpy spy(&viewModel, &ScriptsViewModel::scriptLibraryChanged);

    service.scriptsChanged();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), service.scripts().size());
}

void ScriptsViewModelTest::readsConcreteScriptModel()
{
    ScriptService service;
    const QString decoderId = service.upsertScript(
        {},
        QStringLiteral("Decoder"),
        QStringLiteral("Binary payload"),
        QStringLiteral("function parse(ctx)\nend"));
    const QString loggerId = service.upsertScript(
        {},
        QStringLiteral("Logger"),
        QStringLiteral("Debug event"),
        QStringLiteral("function parse(ctx)\nend"));
    QVERIFY(!decoderId.isEmpty());
    QVERIFY(!loggerId.isEmpty());

    ScriptLibraryModel model;
    ScriptsViewModel viewModel(service, model);

    QCOMPARE(viewModel.scripts(), &model);
    QCOMPARE(model.count(), 2);
    QCOMPARE(viewModel.visibleScriptCount(QStringLiteral("binary")), 1);
    QVERIFY(viewModel.selectScriptAt(1));
    QCOMPARE(viewModel.editor()->currentScriptId(), loggerId);
}

QTEST_MAIN(ScriptsViewModelTest)

#include "test_scriptsviewmodel.moc"
