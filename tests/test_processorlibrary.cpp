#include "services/processors/processorlibrary.h"

#include <QtTest/QtTest>

#include <QTemporaryDir>

namespace {

ProcessorRevisionContent javascriptContent(const QByteArray &source)
{
    ProcessorRevisionContent content;
    content.languageId = QStringLiteral("javascript");
    content.runtimeId = QStringLiteral("qt-qjs");
    content.entryFile = QStringLiteral("main.js");
    content.files = {
        {
            QStringLiteral("main.js"),
            QStringLiteral("text/javascript"),
            source,
            {},
        },
    };
    return content;
}

SaveProcessorRevisionResult save(
    ProcessorLibrary &library,
    const QString &processorId,
    const QByteArray &source)
{
    SaveProcessorRevisionCommand command;
    command.processorId = processorId;
    command.name = QStringLiteral("Device processor");
    command.description = QStringLiteral("");
    command.content = javascriptContent(source);
    return library.saveRevision(command);
}

} // namespace

class ProcessorLibraryTest : public QObject
{
    Q_OBJECT

private slots:
    void resolvesFromAnImmutableInMemorySnapshot();
    void rejectsMissingProcessorReferences();
    void writesThroughTheLibrary();
    void deletesProcessorFromTheLibrary();
};

void ProcessorLibraryTest::resolvesFromAnImmutableInMemorySnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibrary writer(directory.path());
    const SaveProcessorRevisionResult first = save(
        writer,
        {},
        QByteArrayLiteral("function process(context) { return 1 }\n"));
    QVERIFY2(first.ok, qPrintable(first.error));

    ProcessorLibrary library(directory.path());
    QVERIFY2(library.isReady(), qPrintable(library.lastError()));
    ProcessorReference current;
    current.processorId = first.processor.id;
    QString error;
    const auto resolvedFirst = library.resolve(current, &error);
    QVERIFY2(resolvedFirst, qPrintable(error));
    QCOMPARE(resolvedFirst->revision->id, first.revision->id);

    const SaveProcessorRevisionResult second = save(
        writer,
        first.processor.id,
        QByteArrayLiteral("function process(context) { return 2 }\n"));
    QVERIFY2(second.ok, qPrintable(second.error));
    const auto stillFirst = library.resolve(current, &error);
    QVERIFY2(stillFirst, qPrintable(error));
    QCOMPARE(stillFirst->revision->id, first.revision->id);

    QVERIFY2(library.reload(), qPrintable(library.lastError()));
    const auto resolvedSecond = library.resolve(current, &error);
    QVERIFY2(resolvedSecond, qPrintable(error));
    QCOMPARE(resolvedSecond->revision->id, second.revision->id);
    QCOMPARE(resolvedFirst->revision->id, first.revision->id);
}

void ProcessorLibraryTest::rejectsMissingProcessorReferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibrary library(directory.path());
    ProcessorReference missing;
    missing.processorId = QStringLiteral("missing");
    QString error;
    QVERIFY(!library.resolve(missing, &error));
    QVERIFY(error.contains(QStringLiteral("not found")));
}

void ProcessorLibraryTest::writesThroughTheLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibrary library(directory.path());

    SaveProcessorRevisionCommand command;
    command.name = QStringLiteral("Editable processor");
    command.content = javascriptContent(
        QByteArrayLiteral("function process(context) { return context.topic }\n"));
    const SaveProcessorRevisionResult saved = library.saveRevision(command);
    QVERIFY2(saved.ok, qPrintable(saved.error));
    QCOMPARE(library.processors().size(), 1);
    QCOMPARE(library.revisions(saved.processor.id).size(), 1);
    QCOMPARE(library.revisionById(saved.revision->id)->id, saved.revision->id);

}

void ProcessorLibraryTest::deletesProcessorFromTheLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProcessorLibrary library(directory.path());

    SaveProcessorRevisionCommand command;
    command.name = QStringLiteral("Disposable processor");
    command.content = javascriptContent(
        QByteArrayLiteral("function process(context) { return context.payload }\n"));
    const SaveProcessorRevisionResult saved = library.saveRevision(command);
    QVERIFY2(saved.ok, qPrintable(saved.error));

    QVERIFY(library.deleteProcessor(saved.processor.id));
    QVERIFY(library.processors().isEmpty());
    QVERIFY(!library.processorById(saved.processor.id));
    QVERIFY(library.revisionById(saved.revision->id).isNull());
}

QTEST_GUILESS_MAIN(ProcessorLibraryTest)

#include "test_processorlibrary.moc"
