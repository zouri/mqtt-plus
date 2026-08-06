#include "services/processors/processorlibrarystore.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

#include <algorithm>
#include <limits>

namespace {

ProcessorRevisionContent javascriptContent(const QByteArray &mainSource)
{
    ProcessorRevisionContent content;
    content.languageId = QStringLiteral("javascript");
    content.runtimeId = QStringLiteral("qt-qjs");
    content.entryFile = QStringLiteral("src/main.js");
    content.manifest.insert(QStringLiteral("category"), QStringLiteral("device"));
    content.manifest.insert(QStringLiteral("enabled"), true);
    content.files = {
        {
            QStringLiteral("src/main.js"),
            QStringLiteral("text/javascript"),
            mainSource,
            {},
        },
        {
            QStringLiteral("src/lib/protocol.js"),
            QStringLiteral("text/javascript"),
            QByteArrayLiteral("const offset = 4\n"),
            {},
        },
    };
    return content;
}

SaveProcessorRevisionResult saveProcessor(
    ProcessorLibraryStore &store,
    const QString &processorId,
    const QString &name,
    const QByteArray &mainSource)
{
    SaveProcessorRevisionCommand command;
    command.processorId = processorId;
    command.name = name;
    command.description = QStringLiteral("Binary device protocol");
    command.content = javascriptContent(mainSource);
    return store.saveRevision(command);
}

const ProcessorSourceFile *sourceFileByPath(
    const ProcessorRevisionSnapshot &revision,
    const QString &path)
{
    const auto it = std::find_if(
        revision.files.cbegin(),
        revision.files.cend(),
        [&path](const ProcessorSourceFile &file) {
            return file.path == path;
        });
    return it == revision.files.cend() ? nullptr : &*it;
}

} // namespace

class ProcessorLibraryStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void savesImmutableMultiFileRevisionsAndResolvesBindings();
    void reusesIdenticalContentAndCanonicalizesPackageOrder();
    void rejectsInvalidPackagesWithoutChangingTheLibrary();
    void rollsBackARevisionWhenAFileInsertFails();
    void deletesProcessorAndItsRevisions();
    void clearsLegacyArchivedStateOnOpen();
    void rejectsNewerSchemaWithoutOverwritingIt();
};

void ProcessorLibraryStoreTest::savesImmutableMultiFileRevisionsAndResolvesBindings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString processorId;
    QString firstRevisionId;
    {
        ProcessorLibraryStore store(directory.path());
        QVERIFY2(store.isReady(), qPrintable(store.lastError()));

        const SaveProcessorRevisionResult first = saveProcessor(
            store,
            {},
            QStringLiteral("Telemetry"),
            QByteArrayLiteral("function process(context) { return 1 }\n"));
        QVERIFY2(first.ok, qPrintable(first.error));
        QVERIFY(first.createdProcessor);
        QVERIFY(first.createdRevision);
        processorId = first.processor.id;
        firstRevisionId = first.revision->id;
        QCOMPARE(first.revision->revisionNumber, qint64(1));
        QCOMPARE(first.revision->files.size(), 2);
        QVERIFY(!first.revision->contentHash.isEmpty());

        ProcessorReference currentReference;
        currentReference.processorId = processorId;
        QString resolveError;
        const auto currentFirst = store.resolve(currentReference, &resolveError);
        QVERIFY2(currentFirst, qPrintable(resolveError));
        QCOMPARE(currentFirst->id, firstRevisionId);

        const SaveProcessorRevisionResult second = saveProcessor(
            store,
            processorId,
            QStringLiteral("Telemetry"),
            QByteArrayLiteral("function process(context) { return 2 }\n"));
        QVERIFY2(second.ok, qPrintable(second.error));
        QVERIFY(!second.createdProcessor);
        QVERIFY(second.createdRevision);
        QCOMPARE(second.revision->revisionNumber, qint64(2));
        QVERIFY(second.revision->id != firstRevisionId);

        const auto revisions = store.revisions(processorId);
        QCOMPARE(revisions.size(), 2);
        QCOMPARE(revisions.at(0)->id, firstRevisionId);
        const ProcessorSourceFile *firstMain = sourceFileByPath(
            *revisions.at(0),
            QStringLiteral("src/main.js"));
        const ProcessorSourceFile *secondMain = sourceFileByPath(
            *revisions.at(1),
            QStringLiteral("src/main.js"));
        QVERIFY(firstMain);
        QVERIFY(secondMain);
        QVERIFY(firstMain->content.contains("return 1"));
        QVERIFY(secondMain->content.contains("return 2"));

        const auto currentSecond = store.resolve(currentReference, &resolveError);
        QVERIFY2(currentSecond, qPrintable(resolveError));
        QCOMPARE(currentSecond->id, second.revision->id);

    }

    ProcessorLibraryStore reopened(directory.path());
    QVERIFY2(reopened.isReady(), qPrintable(reopened.lastError()));
    const auto revisions = reopened.revisions(processorId);
    QCOMPARE(revisions.size(), 2);
    QCOMPARE(revisions.first()->id, firstRevisionId);
    QCOMPARE(revisions.first()->entryFile, QStringLiteral("src/main.js"));
    QCOMPARE(revisions.first()->manifest.value(QStringLiteral("category")).toString(), QStringLiteral("device"));

    ProcessorRevisionContent reopenedContent;
    reopenedContent.contractId = revisions.first()->contractId;
    reopenedContent.languageId = revisions.first()->languageId;
    reopenedContent.runtimeId = revisions.first()->runtimeId;
    reopenedContent.entryFile = revisions.first()->entryFile;
    reopenedContent.entrySymbol = revisions.first()->entrySymbol;
    reopenedContent.manifest = revisions.first()->manifest;
    reopenedContent.files = revisions.first()->files;
    const PreparedProcessorPackage reopenedPackage = ProcessorPackageHash::prepare(reopenedContent);
    QVERIFY2(reopenedPackage.ok, qPrintable(reopenedPackage.error));
    QCOMPARE(reopenedPackage.contentHash, revisions.first()->contentHash);
}

void ProcessorLibraryStoreTest::reusesIdenticalContentAndCanonicalizesPackageOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibraryStore store(directory.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    SaveProcessorRevisionCommand firstCommand;
    firstCommand.name = QStringLiteral("Canonical");
    firstCommand.description = QStringLiteral("First metadata");
    firstCommand.content = javascriptContent(
        QByteArrayLiteral("function process(context) { return context.topic }\n"));
    const SaveProcessorRevisionResult first = store.saveRevision(firstCommand);
    QVERIFY2(first.ok, qPrintable(first.error));

    SaveProcessorRevisionCommand repeatedCommand = firstCommand;
    repeatedCommand.processorId = first.processor.id;
    repeatedCommand.name = QStringLiteral("Renamed Canonical");
    repeatedCommand.description = QStringLiteral("Updated metadata");
    std::reverse(
        repeatedCommand.content.files.begin(),
        repeatedCommand.content.files.end());
    QCborMap reorderedManifest;
    reorderedManifest.insert(QStringLiteral("enabled"), true);
    reorderedManifest.insert(QStringLiteral("category"), QStringLiteral("device"));
    repeatedCommand.content.manifest = reorderedManifest;

    const PreparedProcessorPackage firstPackage = ProcessorPackageHash::prepare(
        firstCommand.content);
    const PreparedProcessorPackage repeatedPackage = ProcessorPackageHash::prepare(
        repeatedCommand.content);
    QVERIFY2(firstPackage.ok, qPrintable(firstPackage.error));
    QVERIFY2(repeatedPackage.ok, qPrintable(repeatedPackage.error));
    QCOMPARE(repeatedPackage.manifestJson, firstPackage.manifestJson);
    QCOMPARE(repeatedPackage.contentHash, firstPackage.contentHash);

    const SaveProcessorRevisionResult repeated = store.saveRevision(repeatedCommand);
    QVERIFY2(repeated.ok, qPrintable(repeated.error));
    QVERIFY(!repeated.createdProcessor);
    QVERIFY(!repeated.createdRevision);
    QCOMPARE(repeated.revision->id, first.revision->id);
    QCOMPARE(repeated.revision->contentHash, first.revision->contentHash);
    QCOMPARE(store.revisions(first.processor.id).size(), 1);
    QCOMPARE(repeated.processor.name, QStringLiteral("Renamed Canonical"));
    QCOMPARE(repeated.processor.description, QStringLiteral("Updated metadata"));
}

void ProcessorLibraryStoreTest::rejectsInvalidPackagesWithoutChangingTheLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibraryStore store(directory.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    SaveProcessorRevisionCommand traversal;
    traversal.name = QStringLiteral("Traversal");
    traversal.content = javascriptContent(QByteArrayLiteral("function process() {}\n"));
    traversal.content.files.first().path = QStringLiteral("../main.js");
    const SaveProcessorRevisionResult traversalResult = store.saveRevision(traversal);
    QVERIFY(!traversalResult.ok);
    QVERIFY(traversalResult.error.contains(QStringLiteral("invalid segment")));
    QVERIFY(store.processors().isEmpty());

    SaveProcessorRevisionCommand duplicate;
    duplicate.name = QStringLiteral("Duplicate");
    duplicate.content = javascriptContent(QByteArrayLiteral("function process() {}\n"));
    duplicate.content.files.last().path = QStringLiteral("SRC/MAIN.JS");
    const SaveProcessorRevisionResult duplicateResult = store.saveRevision(duplicate);
    QVERIFY(!duplicateResult.ok);
    QVERIFY(duplicateResult.error.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive));
    QVERIFY(store.processors().isEmpty());

    SaveProcessorRevisionCommand missingEntry;
    missingEntry.name = QStringLiteral("Missing Entry");
    missingEntry.content = javascriptContent(QByteArrayLiteral("function process() {}\n"));
    missingEntry.content.entryFile = QStringLiteral("missing.js");
    const SaveProcessorRevisionResult missingEntryResult = store.saveRevision(missingEntry);
    QVERIFY(!missingEntryResult.ok);
    QVERIFY(missingEntryResult.error.contains(QStringLiteral("entry file"), Qt::CaseInsensitive));
    QVERIFY(store.processors().isEmpty());

    SaveProcessorRevisionCommand nonFiniteManifest;
    nonFiniteManifest.name = QStringLiteral("Non-finite manifest");
    nonFiniteManifest.content = javascriptContent(QByteArrayLiteral("function process() {}\n"));
    nonFiniteManifest.content.manifest.insert(
        QStringLiteral("invalidNumber"),
        std::numeric_limits<double>::infinity());
    const SaveProcessorRevisionResult nonFiniteResult = store.saveRevision(nonFiniteManifest);
    QVERIFY(!nonFiniteResult.ok);
    QVERIFY(nonFiniteResult.error.contains(QStringLiteral("finite"), Qt::CaseInsensitive));
    QVERIFY(store.processors().isEmpty());
}

void ProcessorLibraryStoreTest::rollsBackARevisionWhenAFileInsertFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibraryStore store(directory.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    const SaveProcessorRevisionResult saved = saveProcessor(
        store,
        {},
        QStringLiteral("Stable metadata"),
        QByteArrayLiteral("function process() { return 1 }\n"));
    QVERIFY2(saved.ok, qPrintable(saved.error));

    const QString failureConnectionName = QStringLiteral("processor-write-failure-%1")
                                              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), failureConnectionName);
        db.setDatabaseName(store.databasePath());
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QSqlQuery query(db);
        QVERIFY2(query.exec(QStringLiteral(
                     "CREATE TRIGGER reject_processor_file_insert "
                     "BEFORE INSERT ON processor_files "
                     "BEGIN SELECT RAISE(ABORT, 'injected file write failure'); END")),
            qPrintable(query.lastError().text()));
        db.close();
    }
    QSqlDatabase::removeDatabase(failureConnectionName);

    SaveProcessorRevisionCommand failingCommand;
    failingCommand.processorId = saved.processor.id;
    failingCommand.name = QStringLiteral("Metadata must roll back");
    failingCommand.description = QStringLiteral("This update must not commit");
    failingCommand.content = javascriptContent(
        QByteArrayLiteral("function process() { return 2 }\n"));
    const SaveProcessorRevisionResult failed = store.saveRevision(failingCommand);
    QVERIFY(!failed.ok);
    QVERIFY(failed.error.contains(QStringLiteral("injected file write failure")));

    const auto processor = store.processorById(saved.processor.id);
    QVERIFY(processor);
    QCOMPARE(processor->name, QStringLiteral("Stable metadata"));
    QCOMPARE(processor->currentRevisionId, saved.revision->id);
    const auto revisions = store.revisions(saved.processor.id);
    QCOMPARE(revisions.size(), 1);
    QCOMPARE(revisions.first()->id, saved.revision->id);
}

void ProcessorLibraryStoreTest::deletesProcessorAndItsRevisions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibraryStore store(directory.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    const SaveProcessorRevisionResult first = saveProcessor(
        store,
        {},
        QStringLiteral("Disposable"),
        QByteArrayLiteral("function process(context) { return 1 }\n"));
    QVERIFY2(first.ok, qPrintable(first.error));
    const SaveProcessorRevisionResult second = saveProcessor(
        store,
        first.processor.id,
        QStringLiteral("Disposable"),
        QByteArrayLiteral("function process(context) { return 2 }\n"));
    QVERIFY2(second.ok, qPrintable(second.error));
    QCOMPARE(store.revisions(first.processor.id).size(), 2);

    QVERIFY(store.deleteProcessor(first.processor.id));
    QVERIFY(!store.processorById(first.processor.id));
    QVERIFY(store.revisions(first.processor.id).isEmpty());
    QVERIFY(store.revisionById(first.revision->id).isNull());
    QVERIFY(store.revisionById(second.revision->id).isNull());
    QVERIFY(store.processors().isEmpty());
}

void ProcessorLibraryStoreTest::clearsLegacyArchivedStateOnOpen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString databasePath;
    {
        ProcessorLibraryStore store(directory.path());
        QVERIFY2(store.isReady(), qPrintable(store.lastError()));
        const SaveProcessorRevisionResult saved = saveProcessor(
            store,
            {},
            QStringLiteral("Editable processor"),
            QByteArrayLiteral("function process(context) { return true }\n"));
        QVERIFY2(saved.ok, qPrintable(saved.error));
        databasePath = store.databasePath();
    }

    const QString legacyConnectionName = QStringLiteral("legacy-archived-processor");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), legacyConnectionName);
        db.setDatabaseName(databasePath);
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QSqlQuery query(db);
        QVERIFY2(query.exec(QStringLiteral(
            "UPDATE processors SET archived_at = '2026-08-06T06:22:58.452Z'")),
            qPrintable(query.lastError().text()));
        db.close();
    }
    QSqlDatabase::removeDatabase(legacyConnectionName);

    {
        ProcessorLibraryStore reopened(directory.path());
        QVERIFY2(reopened.isReady(), qPrintable(reopened.lastError()));
        QCOMPARE(reopened.processors().size(), 1);
    }

    const QString verificationConnectionName = QStringLiteral("verify-unarchived-processor");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            verificationConnectionName);
        db.setDatabaseName(databasePath);
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QSqlQuery query(db);
        QVERIFY2(query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM processors WHERE archived_at IS NOT NULL")),
            qPrintable(query.lastError().text()));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
        db.close();
    }
    QSqlDatabase::removeDatabase(verificationConnectionName);
}

void ProcessorLibraryStoreTest::rejectsNewerSchemaWithoutOverwritingIt()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = QDir(directory.path()).filePath(QStringLiteral("library.db"));
    const QString connectionName = QStringLiteral("future-processor-schema-%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(databasePath);
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QSqlQuery query(db);
        QVERIFY2(query.exec(QStringLiteral("CREATE TABLE future_marker(value TEXT NOT NULL)")),
            qPrintable(query.lastError().text()));
        QVERIFY2(query.exec(QStringLiteral("INSERT INTO future_marker(value) VALUES('keep')")),
            qPrintable(query.lastError().text()));
        QVERIFY2(query.exec(QStringLiteral("PRAGMA user_version = 99")),
            qPrintable(query.lastError().text()));
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    ProcessorLibraryStore store(directory.path());
    QVERIFY(!store.isReady());
    QVERIFY(store.lastError().contains(QStringLiteral("newer")));

    const QString verificationName = QStringLiteral("verify-future-processor-schema-%1")
                                         .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verificationName);
        db.setDatabaseName(databasePath);
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QSqlQuery query(db);
        QVERIFY2(query.exec(QStringLiteral("SELECT value FROM future_marker")),
            qPrintable(query.lastError().text()));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("keep"));
        db.close();
    }
    QSqlDatabase::removeDatabase(verificationName);
}

QTEST_GUILESS_MAIN(ProcessorLibraryStoreTest)

#include "test_processorlibrarystore.moc"
