#include "services/storage/draftstore.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class DraftStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void savesLoadsAndKeepsBackup();
    void recoversCorruptPrimaryWithoutLosingIt();
    void offersRecoveryWhenPrimaryIsMissing();
    void offersRecoveryForSemanticallyInvalidPrimary();
    void rejectsNewerSchemaVersions();
    void rejectsPreviousSchemaVersion();
    void rejectsOversizedStoredPayload();
    void rejectsUnsupportedQos();
};

namespace {
PublishDraft draft(const QString &id, const QString &name, const QString &payload)
{
    PublishDraft result;
    result.id = id;
    result.name = name;
    result.description = QStringLiteral("Reusable test message");
    result.defaultTopic = QStringLiteral("devices/test/set");
    result.payload = payload;
    result.formatId = QStringLiteral("json");
    result.qos = 2;
    result.retain = true;
    result.properties.contentType = QStringLiteral("application/json");
    result.properties.userProperties.append({QStringLiteral("source"), QStringLiteral("test")});
    result.createdAt = QStringLiteral("2026-08-03T00:00:00.000");
    result.updatedAt = result.createdAt;
    return result;
}

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}
}

void DraftStoreTest::savesLoadsAndKeepsBackup()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QVector<PublishDraft> first {draft(QStringLiteral("one"), QStringLiteral("First"), QStringLiteral("{}"))};
    QVERIFY(DraftStore::saveDrafts(first, temporaryDirectory.path()).ok);

    QFile primaryFile(DraftStore::primaryFilePath(temporaryDirectory.path()));
    QVERIFY2(primaryFile.open(QIODevice::ReadOnly), qPrintable(primaryFile.errorString()));
    const QByteArray storedContent = primaryFile.readAll();
    primaryFile.close();
    const QJsonDocument storedDocument = QJsonDocument::fromJson(storedContent);
    QVERIFY(storedDocument.isObject());
    QCOMPARE(storedDocument.object().value(QStringLiteral("version")).toInt(), 3);

    const DraftStore::LoadResult firstLoad = DraftStore::loadDrafts(temporaryDirectory.path());
    QCOMPARE(firstLoad.state, DraftStore::LoadState::Ready);
    QCOMPARE(firstLoad.drafts.size(), 1);
    QCOMPARE(firstLoad.drafts.first().name, QStringLiteral("First"));
    QCOMPARE(firstLoad.drafts.first().qos, 2);
    QCOMPARE(firstLoad.drafts.first().properties.contentType, QStringLiteral("application/json"));
    QCOMPARE(firstLoad.drafts.first().properties.userProperties.size(), 1);

    const QVector<PublishDraft> second {draft(QStringLiteral("two"), QStringLiteral("Second"), QStringLiteral("{\"v\":2}"))};
    QVERIFY(DraftStore::saveDrafts(second, temporaryDirectory.path()).ok);
    QVERIFY(QFileInfo::exists(DraftStore::backupFilePath(temporaryDirectory.path())));

    const DraftStore::LoadResult secondLoad = DraftStore::loadDrafts(temporaryDirectory.path());
    QCOMPARE(secondLoad.drafts.first().name, QStringLiteral("Second"));
}

void DraftStoreTest::recoversCorruptPrimaryWithoutLosingIt()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QVector<PublishDraft> first {draft(QStringLiteral("one"), QStringLiteral("Recover me"), QStringLiteral("{}"))};
    const QVector<PublishDraft> second {draft(QStringLiteral("two"), QStringLiteral("Replace me"), QStringLiteral("{}"))};
    QVERIFY(DraftStore::saveDrafts(first, temporaryDirectory.path()).ok);
    QVERIFY(DraftStore::saveDrafts(second, temporaryDirectory.path()).ok);
    QVERIFY(writeBytes(DraftStore::primaryFilePath(temporaryDirectory.path()), QByteArray("not json")));

    const DraftStore::LoadResult corrupt = DraftStore::loadDrafts(temporaryDirectory.path());
    QCOMPARE(corrupt.state, DraftStore::LoadState::Corrupt);
    QVERIFY(corrupt.canRecover);

    QVERIFY(DraftStore::recoverBackup(temporaryDirectory.path()).ok);
    const DraftStore::LoadResult recovered = DraftStore::loadDrafts(temporaryDirectory.path());
    QCOMPARE(recovered.state, DraftStore::LoadState::Ready);
    QCOMPARE(recovered.drafts.first().name, QStringLiteral("Recover me"));

    const QStringList corruptArtifacts = QDir(temporaryDirectory.path()).entryList(
        {QStringLiteral("drafts.json.corrupt-*")}, QDir::Files);
    QCOMPARE(corruptArtifacts.size(), 1);
}

void DraftStoreTest::offersRecoveryWhenPrimaryIsMissing()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QVERIFY(DraftStore::saveDrafts(
        {draft(QStringLiteral("one"), QStringLiteral("Recover me"), QStringLiteral("{}"))},
        temporaryDirectory.path()).ok);
    QVERIFY(DraftStore::saveDrafts(
        {draft(QStringLiteral("two"), QStringLiteral("Current"), QStringLiteral("{}"))},
        temporaryDirectory.path()).ok);
    QVERIFY(QFile::remove(DraftStore::primaryFilePath(temporaryDirectory.path())));

    const DraftStore::LoadResult missing = DraftStore::loadDrafts(temporaryDirectory.path());
    QCOMPARE(missing.state, DraftStore::LoadState::Corrupt);
    QVERIFY(missing.canRecover);
    QVERIFY(missing.drafts.isEmpty());

    QVERIFY(DraftStore::recoverBackup(temporaryDirectory.path()).ok);
    const DraftStore::LoadResult recovered = DraftStore::loadDrafts(temporaryDirectory.path());
    QCOMPARE(recovered.state, DraftStore::LoadState::Ready);
    QCOMPARE(recovered.drafts.first().name, QStringLiteral("Recover me"));
}

void DraftStoreTest::offersRecoveryForSemanticallyInvalidPrimary()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(DraftStore::saveDrafts(
        {draft(QStringLiteral("one"), QStringLiteral("Valid backup"), QStringLiteral("{}"))},
        temporaryDirectory.path()).ok);
    QVERIFY(DraftStore::saveDrafts(
        {draft(QStringLiteral("two"), QStringLiteral("Current"), QStringLiteral("{}"))},
        temporaryDirectory.path()).ok);
    QVERIFY(writeBytes(
        DraftStore::primaryFilePath(temporaryDirectory.path()),
        QByteArray(
            "{\"version\":2,\"drafts\":[{\"id\":\"broken\",\"name\":\"Broken\","
            "\"description\":\"\",\"defaultTopic\":\"devices/+/set\",\"payload\":\"\","
            "\"format\":\"text\",\"qos\":0,\"retain\":false,\"createdAt\":\"\","
            "\"updatedAt\":\"\",\"lastUsedAt\":\"\"}]}")));

    const DraftStore::LoadResult result = DraftStore::loadDrafts(temporaryDirectory.path());
    QCOMPARE(result.state, DraftStore::LoadState::Corrupt);
    QVERIFY(result.canRecover);
}

void DraftStoreTest::rejectsNewerSchemaVersions()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(DraftStore::saveDrafts(
        {draft(QStringLiteral("one"), QStringLiteral("Older"), QStringLiteral("{}"))},
        temporaryDirectory.path()).ok);
    QVERIFY(DraftStore::saveDrafts(
        {draft(QStringLiteral("two"), QStringLiteral("Current"), QStringLiteral("{}"))},
        temporaryDirectory.path()).ok);
    QVERIFY(writeBytes(
        DraftStore::primaryFilePath(temporaryDirectory.path()),
        QByteArray("{\"version\":99,\"drafts\":[]}")));

    const DraftStore::LoadResult result = DraftStore::loadDrafts(temporaryDirectory.path());
    QCOMPARE(result.state, DraftStore::LoadState::Incompatible);
    QVERIFY(result.drafts.isEmpty());
    QVERIFY(!result.canRecover);
    QVERIFY(!result.errorMessage.isEmpty());
    QVERIFY(!DraftStore::recoverBackup(temporaryDirectory.path()).ok);

    const DraftStore::LoadResult stillNewer = DraftStore::loadDrafts(temporaryDirectory.path());
    QCOMPARE(stillNewer.state, DraftStore::LoadState::Incompatible);
}

void DraftStoreTest::rejectsPreviousSchemaVersion()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(writeBytes(
        DraftStore::primaryFilePath(temporaryDirectory.path()),
        QByteArray("{\"version\":1,\"drafts\":[]}")));

    const DraftStore::LoadResult result = DraftStore::loadDrafts(temporaryDirectory.path());
    QCOMPARE(result.state, DraftStore::LoadState::Corrupt);
    QVERIFY(result.drafts.isEmpty());
    QVERIFY(!result.canRecover);
    QVERIFY(!result.errorMessage.isEmpty());
}

void DraftStoreTest::rejectsOversizedStoredPayload()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PublishDraft oversized = draft(
        QStringLiteral("oversized"),
        QStringLiteral("Oversized"),
        QString(16 * 1024 * 1024 + 1, QLatin1Char(' ')) + QStringLiteral("00"));
    oversized.formatId = QStringLiteral("hex");

    const DraftStore::SaveResult result = DraftStore::saveDrafts(
        {oversized}, temporaryDirectory.path());
    QVERIFY(!result.ok);
    QVERIFY(result.errorMessage.contains(QStringLiteral("oversized"), Qt::CaseInsensitive));
}

void DraftStoreTest::rejectsUnsupportedQos()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    PublishDraft invalidQos = draft(
        QStringLiteral("invalid-qos"),
        QStringLiteral("Invalid QoS"),
        QStringLiteral("{}"));
    invalidQos.qos = 3;
    const DraftStore::SaveResult invalidQosResult = DraftStore::saveDrafts(
        {invalidQos}, temporaryDirectory.path());
    QVERIFY(!invalidQosResult.ok);
    QVERIFY(invalidQosResult.errorMessage.contains(QStringLiteral("QoS")));
}

QTEST_MAIN(DraftStoreTest)

#include "test_draftstore.moc"
