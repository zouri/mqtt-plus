#include "usecases/draftlibraryservice.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class DraftLibraryServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void createsUpdatesTouchesAndDeletesDurably();
    void enforcesNamesTopicsAndEncodedPayloads();
    void importsDraftsInOneAtomicSave();
    void suggestsUniqueCopyNameAtMaximumLength();
    void leavesVisibleLibraryUnchangedWhenSaveFails();
};

void DraftLibraryServiceTest::createsUpdatesTouchesAndDeletesDurably()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    DraftLibraryService service(temporaryDirectory.path());
    service.load();
    QTRY_VERIFY(service.ready());

    QSignalSpy operationSpy(&service, &DraftLibraryService::operationSucceeded);
    PublishDraft created;
    created.name = QStringLiteral("Reset device");
    created.payload = QString();
    created.formatId = QStringLiteral("json");
    created.qos = 2;
    QVERIFY(service.createDraft(created));
    QVERIFY(service.busy());
    QCOMPARE(service.drafts().size(), 0);
    QTRY_COMPARE(operationSpy.size(), 1);
    QCOMPARE(service.drafts().size(), 1);
    QCOMPARE(service.drafts().first().qos, 2);

    PublishDraft updated = service.drafts().first();
    updated.description = QStringLiteral("Factory reset request");
    updated.defaultTopic = QStringLiteral("devices/one/reset");
    QVERIFY(service.updateDraft(updated));
    QTRY_COMPARE(operationSpy.size(), 2);
    QCOMPARE(service.drafts().first().description, QStringLiteral("Factory reset request"));

    service.markUsed(updated.id);
    QTRY_COMPARE(operationSpy.size(), 3);
    QVERIFY(!service.drafts().first().lastUsedAt.isEmpty());

    QVERIFY(service.removeDraft(updated.id));
    QTRY_COMPARE(operationSpy.size(), 4);
    QVERIFY(service.drafts().isEmpty());

    DraftLibraryService reloaded(temporaryDirectory.path());
    reloaded.load();
    QTRY_VERIFY(reloaded.ready());
    QVERIFY(reloaded.drafts().isEmpty());
}

void DraftLibraryServiceTest::enforcesNamesTopicsAndEncodedPayloads()
{
    QTemporaryDir temporaryDirectory;
    DraftLibraryService service(temporaryDirectory.path());
    service.load();
    QTRY_VERIFY(service.ready());

    PublishDraft first;
    first.name = QStringLiteral("Heartbeat");
    first.formatId = QStringLiteral("json");
    QVERIFY(service.createDraft(first));
    QTRY_COMPARE(service.drafts().size(), 1);

    PublishDraft duplicate = first;
    duplicate.id.clear();
    duplicate.name = QStringLiteral("  heartbeat  ");
    QVERIFY(!service.createDraft(duplicate));
    QVERIFY(service.errorMessage().contains(QStringLiteral("already exists")));

    PublishDraft invalidTopic;
    invalidTopic.name = QStringLiteral("Invalid topic");
    invalidTopic.defaultTopic = QStringLiteral("devices/+/set");
    invalidTopic.formatId = QStringLiteral("text");
    QString error;
    QVERIFY(!service.validateDraft(invalidTopic, error));
    QVERIFY(error.contains(QStringLiteral("topic"), Qt::CaseInsensitive));

    PublishDraft invalidPayload;
    invalidPayload.name = QStringLiteral("Invalid JSON");
    invalidPayload.payload = QStringLiteral("not json");
    invalidPayload.formatId = QStringLiteral("json");
    QVERIFY(!service.validateDraft(invalidPayload, error));
    QVERIFY(!error.isEmpty());

    PublishDraft oversizedStoredPayload;
    oversizedStoredPayload.name = QStringLiteral("Oversized raw Hex");
    oversizedStoredPayload.payload = QString(16 * 1024 * 1024 + 1, QLatin1Char(' '))
        + QStringLiteral("00");
    oversizedStoredPayload.formatId = QStringLiteral("hex");
    QVERIFY(!service.validateDraft(oversizedStoredPayload, error));
    QVERIFY(error.contains(QStringLiteral("16 MiB")));

    PublishDraft invalidQos;
    invalidQos.name = QStringLiteral("Invalid QoS");
    invalidQos.formatId = QStringLiteral("text");
    invalidQos.qos = 3;
    QVERIFY(!service.validateDraft(invalidQos, error));
    QVERIFY(error.contains(QStringLiteral("0, 1, or 2")));
}

void DraftLibraryServiceTest::importsDraftsInOneAtomicSave()
{
    QTemporaryDir temporaryDirectory;
    DraftLibraryService service(temporaryDirectory.path());
    service.load();
    QTRY_VERIFY(service.ready());

    PublishDraft first;
    first.id = QStringLiteral("import-one");
    first.name = QStringLiteral("Imported one");
    first.payload = QStringLiteral("hello");
    first.formatId = QStringLiteral("text");

    PublishDraft second;
    second.id = QStringLiteral("import-two");
    second.name = QStringLiteral("Imported two");
    second.payload = QStringLiteral("{}");
    second.formatId = QStringLiteral("json");
    second.qos = 2;

    QSignalSpy operationSpy(&service, &DraftLibraryService::operationSucceeded);
    QVERIFY(service.importDrafts({first, second}));
    QVERIFY(service.busy());
    QCOMPARE(service.drafts().size(), 0);
    QTRY_COMPARE(operationSpy.size(), 1);
    QCOMPARE(operationSpy.first().first().toString(), QStringLiteral("import"));
    QCOMPARE(service.drafts().size(), 2);

    DraftLibraryService reloaded(temporaryDirectory.path());
    reloaded.load();
    QTRY_VERIFY(reloaded.ready());
    QCOMPARE(reloaded.drafts().size(), 2);
}

void DraftLibraryServiceTest::suggestsUniqueCopyNameAtMaximumLength()
{
    QTemporaryDir temporaryDirectory;
    DraftLibraryService service(temporaryDirectory.path());
    service.load();
    QTRY_VERIFY(service.ready());

    PublishDraft original;
    original.name = QString(80, QLatin1Char('a'));
    original.formatId = QStringLiteral("text");
    QVERIFY(service.createDraft(original));
    QTRY_COMPARE(service.drafts().size(), 1);

    const QString copyName = service.suggestCopyName(original.name);
    QVERIFY(copyName.size() <= 80);
    QVERIFY(copyName.compare(original.name, Qt::CaseInsensitive) != 0);

    PublishDraft copy = original;
    copy.id.clear();
    copy.name = copyName;
    QVERIFY(service.createDraft(copy));
    QTRY_COMPARE(service.drafts().size(), 2);
}

void DraftLibraryServiceTest::leavesVisibleLibraryUnchangedWhenSaveFails()
{
    QTemporaryDir temporaryDirectory;
    const QString blockedRoot = temporaryDirectory.filePath(QStringLiteral("not-a-directory"));
    QFile blocker(blockedRoot);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.write("x");
    blocker.close();

    DraftLibraryService service(blockedRoot);
    service.load();
    QTRY_VERIFY(service.ready());
    QSignalSpy storageErrorSpy(&service, &DraftLibraryService::storageError);

    PublishDraft draft;
    draft.name = QStringLiteral("Cannot persist");
    draft.formatId = QStringLiteral("text");
    QVERIFY(service.createDraft(draft));
    QTRY_COMPARE(storageErrorSpy.size(), 1);
    QVERIFY(!service.busy());
    QVERIFY(service.drafts().isEmpty());
}

QTEST_MAIN(DraftLibraryServiceTest)

#include "test_draftlibraryservice.moc"
