#include "models/processorlibrarymodel.h"
#include "services/processors/processorlibrary.h"
#include "viewmodels/processorsviewmodel.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>

class ProcessorsViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void createsValidLuaAndJavaScriptRevisions();
    void switchesCurrentRevisionWithoutMutatingHistory();
    void preservesUnexposedSourceFilesWhenSavingEntry();
    void filtersAndArchivesProcessors();
};

void ProcessorsViewModelTest::createsValidLuaAndJavaScriptRevisions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibrary library(directory.path());
    ProcessorLibraryModel model;
    ProcessorsViewModel viewModel(library, model);

    viewModel.newProcessor(QStringLiteral("lua"));
    QCOMPARE(viewModel.editor()->languageId(), QStringLiteral("lua"));
    QCOMPARE(viewModel.editor()->runtimeId(), QStringLiteral("lua-5.5"));
    QVERIFY(viewModel.validateEditor());
    QVERIFY(viewModel.saveEditor());
    const QString luaProcessorId = viewModel.editor()->currentProcessorId();
    QVERIFY(!luaProcessorId.isEmpty());
    QCOMPARE(library.revisions(luaProcessorId).size(), 1);

    viewModel.newProcessor(QStringLiteral("javascript"));
    QCOMPARE(viewModel.editor()->languageId(), QStringLiteral("javascript"));
    QCOMPARE(viewModel.editor()->runtimeId(), QStringLiteral("qt-qjs"));
    QVERIFY(viewModel.validateEditor());
    QVERIFY(viewModel.saveEditor());
    QCOMPARE(model.count(), 2);
    QCOMPARE(library.revisions(viewModel.editor()->currentProcessorId()).size(), 1);
}

void ProcessorsViewModelTest::switchesCurrentRevisionWithoutMutatingHistory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibrary library(directory.path());
    ProcessorLibraryModel model;
    ProcessorsViewModel viewModel(library, model);

    viewModel.newProcessor(QStringLiteral("lua"));
    QVERIFY(viewModel.saveEditor());
    const QString processorId = viewModel.editor()->currentProcessorId();
    const QString firstRevisionId = viewModel.editor()->currentRevisionId();

    viewModel.editor()->setSource(QStringLiteral(
        "function process(context)\n"
        "    return { topic = context.topic }\n"
        "end\n"));
    QVERIFY(viewModel.saveEditor());
    const QString secondRevisionId = viewModel.editor()->currentRevisionId();
    QVERIFY(secondRevisionId != firstRevisionId);
    QCOMPARE(library.revisions(processorId).size(), 2);

    viewModel.editor()->setLanguageIndex(1);
    QCOMPARE(viewModel.editor()->languageId(), QStringLiteral("javascript"));
    QCOMPARE(viewModel.editor()->entryFile(), QStringLiteral("main.js"));
    QVERIFY(viewModel.saveEditor());
    QCOMPARE(library.revisions(processorId).size(), 3);
    QCOMPARE(library.revisionById(firstRevisionId)->languageId, QStringLiteral("lua"));
    QCOMPARE(library.revisionById(secondRevisionId)->languageId, QStringLiteral("lua"));
    QCOMPARE(
        library.revisionById(viewModel.editor()->currentRevisionId())->languageId,
        QStringLiteral("javascript"));

    QVERIFY(viewModel.selectRevisionAt(2));
    QCOMPARE(viewModel.editor()->selectedRevisionId(), firstRevisionId);
    QVERIFY(viewModel.editor()->canSave());
    QVERIFY(viewModel.saveEditor());
    QCOMPARE(viewModel.editor()->currentRevisionId(), firstRevisionId);
    QCOMPARE(library.revisions(processorId).size(), 3);
}

void ProcessorsViewModelTest::preservesUnexposedSourceFilesWhenSavingEntry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibrary library(directory.path());
    SaveProcessorRevisionCommand command;
    command.name = QStringLiteral("Multi-file processor");
    command.content.languageId = QStringLiteral("javascript");
    command.content.runtimeId = QStringLiteral("qt-qjs");
    command.content.entryFile = QStringLiteral("main.js");
    command.content.files = {
        {
            QStringLiteral("helper.js"),
            QStringLiteral("text/javascript"),
            QByteArrayLiteral("function helper() { return 1; }\n"),
            {},
        },
        {
            QStringLiteral("main.js"),
            QStringLiteral("text/javascript"),
            QByteArrayLiteral("function process(context) { return helper(); }\n"),
            {},
        },
    };
    const SaveProcessorRevisionResult first = library.saveRevision(command);
    QVERIFY2(first.ok, qPrintable(first.error));

    ProcessorLibraryModel model;
    ProcessorsViewModel viewModel(library, model);
    viewModel.ensureEditorSelection();
    QCOMPARE(viewModel.editor()->sourceFiles().size(), 2);
    viewModel.editor()->setSource(QStringLiteral(
        "function process(context) { return helper() + 1; }\n"));
    QVERIFY(viewModel.saveEditor());

    const auto revisions = library.revisions(first.processor.id);
    QCOMPARE(revisions.size(), 2);
    QCOMPARE(revisions.last()->files.size(), 2);
    const auto helper = std::find_if(
        revisions.last()->files.cbegin(),
        revisions.last()->files.cend(),
        [](const ProcessorSourceFile &file) {
            return file.path == QStringLiteral("helper.js");
        });
    QVERIFY(helper != revisions.last()->files.cend());
    QCOMPARE(helper->content, QByteArrayLiteral("function helper() { return 1; }\n"));
}

void ProcessorsViewModelTest::filtersAndArchivesProcessors()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibrary library(directory.path());
    ProcessorLibraryModel model;
    ProcessorsViewModel viewModel(library, model);

    viewModel.newProcessor(QStringLiteral("javascript"));
    viewModel.editor()->setName(QStringLiteral("Telemetry Formatter"));
    viewModel.editor()->setDescription(QStringLiteral("Vehicle frames"));
    QVERIFY(viewModel.saveEditor());

    viewModel.setProcessorFilterText(QStringLiteral("vehicle"));
    QCOMPARE(viewModel.filteredProcessors()->count(), 1);
    viewModel.setProcessorFilterText(QStringLiteral("JavaScript"));
    QCOMPARE(viewModel.filteredProcessors()->count(), 1);
    viewModel.setProcessorFilterText(QStringLiteral("context.decoded"));
    QCOMPARE(viewModel.filteredProcessors()->count(), 1);

    QVERIFY(viewModel.archiveCurrent());
    QVERIFY(viewModel.editor()->archived());
    QCOMPARE(model.rowAt(0).value(QStringLiteral("archived")).toBool(), true);
    QVERIFY(viewModel.restoreCurrent());
    QVERIFY(!viewModel.editor()->archived());
}

QTEST_MAIN(ProcessorsViewModelTest)

#include "test_processorsviewmodel.moc"
