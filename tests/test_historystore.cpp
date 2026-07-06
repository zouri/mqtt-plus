#include "services/storage/historystore.h"

#include <QtTest/QtTest>

#include <QTemporaryDir>
#include <QUuid>

class HistoryStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void flushesRawPayloadWithoutNullBase64();
    void loadMessagesUsesPreviewWithoutPayloadBytes();
    void loadsPayloadBytesByMessageId();
};

void HistoryStoreTest::flushesRawPayloadWithoutNullBase64()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    const QString sessionId = QStringLiteral("test-session-%1")
                                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QByteArray payload;
    payload.append('\0');
    payload.append(char(0xff));
    payload.append("raw");

    const qint64 reservedId = store.enqueueMessage(
        sessionId,
        QStringLiteral("2026-07-02 15:02:20.304"),
        QStringLiteral("111111/ros_to_android"),
        payload,
        QString(),
        QString(),
        QString(),
        QString(),
        QString(),
        QStringLiteral("raw preview"),
        QStringLiteral("full"),
        payload.size(),
        QStringLiteral("hash"));

    QVERIFY(reservedId > 0);
    QCOMPARE(store.pendingMessageCount(), 1);

    QCOMPARE(store.flushPendingMessages(), QStringList({sessionId}));
    QVERIFY2(store.lastError().isEmpty(), qPrintable(store.lastError()));
    QCOMPARE(store.pendingMessageCount(), 0);

    const QVariantList rows = store.loadMessages(sessionId, 10);
    QCOMPARE(rows.size(), 1);

    const QVariantMap row = rows.first().toMap();
    QCOMPARE(row.value(QStringLiteral("payload_b64")).toString(), QStringLiteral(""));
    QCOMPARE(row.value(QStringLiteral("payload_bytes")).toByteArray(), QByteArray());
    QCOMPARE(row.value(QStringLiteral("payload_size")).toLongLong(), qint64(payload.size()));
}

void HistoryStoreTest::loadMessagesUsesPreviewWithoutPayloadBytes()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    const QString sessionId = QStringLiteral("test-session-%1")
                                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QByteArray payload(1024 * 32, 'x');

    const qint64 reservedId = store.enqueueMessage(
        sessionId,
        QStringLiteral("2026-07-02T15:02:20.304Z"),
        QStringLiteral("devices/large"),
        payload,
        QString(),
        QString(),
        QString(),
        QString(),
        QString(),
        QStringLiteral("preview only"),
        QStringLiteral("full"),
        payload.size(),
        QStringLiteral("hash"));

    QVERIFY(reservedId > 0);
    QCOMPARE(store.flushPendingMessages(), QStringList({sessionId}));

    const QVariantList rows = store.loadMessages(sessionId, 10);
    QCOMPARE(rows.size(), 1);

    const QVariantMap row = rows.first().toMap();
    QCOMPARE(row.value(QStringLiteral("payload_preview")).toString(), QStringLiteral("preview only"));
    QCOMPARE(row.value(QStringLiteral("payload_state")).toString(), QStringLiteral("full"));
    QCOMPARE(row.value(QStringLiteral("payload_size")).toLongLong(), qint64(payload.size()));
    QCOMPARE(row.value(QStringLiteral("payload_bytes")).toByteArray(), QByteArray());
}

void HistoryStoreTest::loadsPayloadBytesByMessageId()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());

    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));

    const QString sessionId = QStringLiteral("test-session-%1")
                                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QByteArray payload("full payload");

    const qint64 reservedId = store.enqueueMessage(
        sessionId,
        QStringLiteral("2026-07-02T15:02:20.304Z"),
        QStringLiteral("devices/payload"),
        payload,
        QString(),
        QString(),
        QString(),
        QString(),
        QString(),
        QStringLiteral("preview"),
        QStringLiteral("full"),
        payload.size(),
        QStringLiteral("hash"));

    QVERIFY(reservedId > 0);
    QCOMPARE(store.flushPendingMessages(), QStringList({sessionId}));
    QCOMPARE(store.loadMessagePayloadBytes(reservedId), payload);
}

QTEST_MAIN(HistoryStoreTest)

#include "test_historystore.moc"
