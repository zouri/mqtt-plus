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
    void savesChangesWithoutExposingRevisionHistory();
    void preservesUnexposedSourceFilesWhenSavingEntry();
    void filtersProcessors();
    void blocksDeletionWhileUsedAndDeletesAfterUnbinding();
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

void ProcessorsViewModelTest::savesChangesWithoutExposingRevisionHistory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibrary library(directory.path());
    ProcessorLibraryModel model;
    ProcessorsViewModel viewModel(library, model);

    viewModel.newProcessor(QStringLiteral("lua"));
    QVERIFY(viewModel.saveEditor());
    const QString processorId = viewModel.editor()->currentProcessorId();
    const QString firstRevisionId = library.processorById(processorId)->currentRevisionId;

    viewModel.editor()->setSource(QStringLiteral(
        "function process(context)\n"
        "    return { topic = context.topic }\n"
        "end\n"));
    QVERIFY(viewModel.saveEditor());
    const QString secondRevisionId = library.processorById(processorId)->currentRevisionId;
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
        library.revisionById(library.processorById(processorId)->currentRevisionId)->languageId,
        QStringLiteral("javascript"));
    const QVariantMap processorRow = model.rowAt(model.indexOfId(processorId));
    QVERIFY(!processorRow.contains(QStringLiteral("currentRevisionNumber")));
    QVERIFY(!processorRow.contains(QStringLiteral("revisions")));
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

void ProcessorsViewModelTest::filtersProcessors()
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

    QVERIFY(!model.rowAt(0).contains(QStringLiteral("archived")));
}

void ProcessorsViewModelTest::blocksDeletionWhileUsedAndDeletesAfterUnbinding()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibrary library(directory.path());
    ProcessorLibraryModel model;
    bool used = true;
    ProcessorsViewModel viewModel(
        library,
        model,
        [&used](const QString &) {
            return used ? QStringList {QStringLiteral("Production")} : QStringList {};
        });

    viewModel.newProcessor(QStringLiteral("lua"));
    viewModel.editor()->setName(QStringLiteral("Bound processor"));
    QVERIFY(viewModel.saveEditor());
    const QString processorId = viewModel.editor()->currentProcessorId();
    const QString revisionId = library.processorById(processorId)->currentRevisionId;

    QVERIFY(!viewModel.deleteCurrent());
    QVERIFY(library.processorById(processorId));
    QVERIFY(viewModel.editor()->diagnostics().contains(QStringLiteral("Production")));

    used = false;
    QVERIFY(viewModel.deleteCurrent());
    QVERIFY(!library.processorById(processorId));
    QVERIFY(library.revisionById(revisionId).isNull());
    QCOMPARE(model.count(), 0);
    QVERIFY(viewModel.editor()->currentProcessorId().isEmpty());
}

QTEST_MAIN(ProcessorsViewModelTest)

#include "test_processorsviewmodel.moc"
