#include "services/storage/editordraftstore.h"
#include "services/processors/editorcodec.h"
#include <QJsonArray>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

class EditorDraftStoreTest : public QObject
{
    Q_OBJECT
private slots:
    void persistsAndRejectsStaleWrites();
    void rejectsMetadataConflictsAndReplaysSaves();
    void preservesTypingDuringSave();
    void detachesDeletedProcessorDrafts();
    void rejectsInvalidPackages();
    void rejectsFutureSchemas();
    void enforcesDraftCountQuota();
};

void EditorDraftStoreTest::persistsAndRejectsStaleWrites()
{
    QTemporaryDir directory;
    QJsonObject draft;
    {
        EditorDraftStore store(directory.path());
        auto created = store.request("POST", "/drafts", {{"languageId", "lua"}});
        QCOMPARE(created.status, 201);
        draft = created.data;
        const auto id = draft.value("draftId").toString();
        draft.insert("description", "Synthetic test draft");
        auto updated = store.request("PUT", "/drafts/" + id, draft, "1");
        QCOMPARE(updated.status, 200);
        QCOMPARE(updated.data.value("draftVersion").toString(), QStringLiteral("2"));
        QCOMPARE(store.request("PUT", "/drafts/" + id, draft, "1").status, 409);
        QCOMPARE(store.request("DELETE", "/drafts/" + id, {}, "1").status, 409);
    }
    EditorDraftStore reopened(directory.path());
    const auto loaded = reopened.request("GET", "/drafts/" + draft.value("draftId").toString());
    QCOMPARE(loaded.status, 200);
    QCOMPARE(loaded.data.value("description").toString(), QStringLiteral("Synthetic test draft"));
}

void EditorDraftStoreTest::rejectsMetadataConflictsAndReplaysSaves()
{
    QTemporaryDir directory;
    EditorDraftStore store(directory.path());
    const auto draft = store.request("POST", "/drafts", {{"languageId", "javascript"}}).data;
    const auto saved = store.save(draft, "save-one");
    QCOMPARE(saved.status, 200);
    QVERIFY(saved.saved.ok);
    const auto repeated = store.save(draft, "save-one");
    QCOMPARE(repeated.status, 200);
    QCOMPARE(store.library().processors().size(), 1);
    QCOMPARE(repeated.saved.revision->id, saved.saved.revision->id);
    auto different = draft;
    different.insert("description", "Changed request");
    QCOMPARE(store.save(different, "save-one").status, 409);
    const auto id = saved.saved.processor.id;
    auto first = store.request("POST", "/drafts", {{"processorId", id}}).data;
    auto second = store.request("POST", "/drafts", {{"processorId", id}}).data;
    first.insert("description", "Metadata only");
    QCOMPARE(store.save(first, "save-two").status, 200);
    QCOMPARE(store.save(second, "save-three").status, 409);
    const auto latest = store.library().processorById(id);
    QVERIFY(latest);
    QCOMPARE(store.save(draft, "save-one").status, 200);
    QCOMPARE(store.library().processorById(id)->changeSequence, latest->changeSequence);
    QCOMPARE(store.library().processorById(id)->description, latest->description);
    QCOMPARE(store.request("GET", "/requests/save-one").status, 200);
}

void EditorDraftStoreTest::preservesTypingDuringSave()
{
    QTemporaryDir directory;
    EditorDraftStore store(directory.path());
    const auto frozen = store.request("POST", "/drafts", {{"languageId", "lua"}}).data;
    auto newer = frozen;
    newer.insert("description", "Typing while validation runs");
    const auto id = frozen.value("draftId").toString();
    QCOMPARE(store.request("PUT", "/drafts/" + id, newer, "1").status, 200);
    const auto saved = store.save(frozen, "concurrent-save");
    QVERIFY(saved.saved.ok);
    const auto survivor = store.request("GET", "/drafts/" + id);
    QCOMPARE(survivor.status, 200);
    QCOMPARE(survivor.data.value("description"), newer.value("description"));
    QCOMPARE(survivor.data.value("processorId").toString(), saved.saved.processor.id);
    QCOMPARE(survivor.data.value("baseVersion").toString(), QString::number(saved.saved.processor.changeSequence));
    const auto replay = store.save(frozen, "concurrent-save");
    QCOMPARE(replay.status, 200);
    QVERIFY(replay.saved.replayed);
    QCOMPARE(store.request("GET", "/drafts/" + id).data, survivor.data);
    QCOMPARE(store.draftCounts().value(saved.saved.processor.id).toInt(), 1);
    const auto summaries = store.request("GET", "/drafts", {}, {}, saved.saved.processor.id).data.value("drafts").toArray();
    QCOMPARE(summaries.size(), 1);
    QVERIFY(!summaries.first().toObject().contains("files"));
    QVERIFY(!summaries.first().toObject().contains("manifest"));
    QCOMPARE(summaries.first().toObject().value("fileCount").toInt(), 1);
    QCOMPARE(summaries.first().toObject().value("baseVersion"), survivor.data.value("baseVersion"));
}

void EditorDraftStoreTest::detachesDeletedProcessorDrafts()
{
    QTemporaryDir directory;
    EditorDraftStore store(directory.path());
    const auto first = store.request("POST", "/drafts", {{"languageId", "lua"}}).data;
    const auto saved = store.save(first, "initial");
    const auto id = saved.saved.processor.id;
    const auto draft = store.request("POST", "/drafts", {{"processorId", id}}).data;
    QCOMPARE(store.deleteProcessor(id).status, 200);
    const auto detached = store.request("GET", "/drafts/" + draft.value("draftId").toString());
    QCOMPARE(detached.status, 200);
    QVERIFY(detached.data.value("processorId").toString().isEmpty());
    QCOMPARE(detached.data.value("files"), draft.value("files"));
    QCOMPARE(store.save(draft, "deleted-save").status, 410);
    QCOMPARE(store.request("GET", "/drafts").data.value("drafts").toArray().size(), 1);
}

void EditorDraftStoreTest::rejectsInvalidPackages()
{
    QTemporaryDir directory;
    EditorDraftStore store(directory.path());
    auto draft = EditorCodec::templateDraft("lua");
    auto files = draft.value("files").toArray();
    auto file = files.first().toObject();
    file.insert("path", "../outside.lua");
    files[0] = file;
    draft.insert("files", files);
    QCOMPARE(store.request("POST", "/drafts", draft).status, 422);
    file.insert("path", "main.lua");
    file.insert("content", QString(1024 * 1024 + 1, QLatin1Char('x')));
    files[0] = file;
    draft.insert("files", files);
    QCOMPARE(store.request("POST", "/drafts", draft).status, 422);
}

void EditorDraftStoreTest::rejectsFutureSchemas()
{
    QTemporaryDir directory;
    { EditorDraftStore initialized(directory.path()); QVERIFY(initialized.isReady()); }
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("future-draft-schema"));
        db.setDatabaseName(directory.filePath(QStringLiteral("editor-drafts.db")));
        QVERIFY(db.open());
        QSqlQuery query(db);
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 2")));
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("future-draft-schema"));
    EditorDraftStore unsupported(directory.path());
    QVERIFY(!unsupported.isReady());
    QCOMPARE(unsupported.request("POST", "/drafts", {{"languageId", "lua"}}).status, 503);
}

void EditorDraftStoreTest::enforcesDraftCountQuota()
{
    QTemporaryDir directory;
    EditorDraftStore store(directory.path());
    QJsonObject first;
    for (int i = 0; i < 100; ++i) {
        const auto created = store.request("POST", "/drafts", {{"languageId", "lua"}});
        QCOMPARE(created.status, 201);
        if (i == 0) first = created.data;
    }
    QCOMPARE(store.request("POST", "/drafts", {{"languageId", "lua"}}).status, 413);
    first.insert("description", "Update at capacity");
    QCOMPARE(store.request("PUT", "/drafts/" + first.value("draftId").toString(), first, "1").status, 200);
    QCOMPARE(store.request("GET", "/drafts").data.value("drafts").toArray().size(), 100);
}

QTEST_GUILESS_MAIN(EditorDraftStoreTest)
#include "test_editordraftstore.moc"
