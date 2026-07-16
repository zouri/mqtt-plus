#include "app/messageretentionlifecycle.h"
#include "services/storage/historystore.h"

#include <QtTest/QtTest>

#include <QTemporaryDir>

class MessageRetentionLifecycleTest : public QObject
{
    Q_OBJECT

private slots:
    void retainsNewestRowsForEverySession();
    void zeroLimitLeavesRowsUntouched();
    void exitFlushesPendingRowsBeforeRetention();
    void exitAllCleanupRemainsAuthoritative();

private:
    static void enqueueMessages(HistoryStore &store, const QString &sessionId, int first, int count);
    static QStringList storedTopics(HistoryStore &store, const QString &sessionId);
};

void MessageRetentionLifecycleTest::enqueueMessages(
    HistoryStore &store,
    const QString &sessionId,
    int first,
    int count)
{
    for (int index = first; index < first + count; ++index) {
        store.enqueueMessage(
            sessionId,
            QStringLiteral("2026-07-16T00:00:%1Z").arg(index, 2, 10, QLatin1Char('0')),
            QStringLiteral("topic/%1").arg(index),
            QByteArray::number(index));
    }
}

QStringList MessageRetentionLifecycleTest::storedTopics(HistoryStore &store, const QString &sessionId)
{
    QStringList topics;
    const QVariantList rows = store.loadMessages(sessionId, 100);
    for (const QVariant &row : rows) {
        topics.append(row.toMap().value(QStringLiteral("topic")).toString());
    }
    return topics;
}

void MessageRetentionLifecycleTest::retainsNewestRowsForEverySession()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());
    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));
    MessageRetentionLifecycle lifecycle(store);

    enqueueMessages(store, QStringLiteral("session-a"), 0, 5);
    enqueueMessages(store, QStringLiteral("session-b"), 10, 5);
    QCOMPARE(store.flushPendingMessages().size(), 2);

    lifecycle.applyRetention(
        {
            SessionState {.id = QStringLiteral("session-a")},
            SessionState {.id = QStringLiteral("session-b")},
        },
        2);

    QCOMPARE(storedTopics(store, QStringLiteral("session-a")),
        QStringList({QStringLiteral("topic/3"), QStringLiteral("topic/4")}));
    QCOMPARE(storedTopics(store, QStringLiteral("session-b")),
        QStringList({QStringLiteral("topic/13"), QStringLiteral("topic/14")}));
}

void MessageRetentionLifecycleTest::zeroLimitLeavesRowsUntouched()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());
    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));
    MessageRetentionLifecycle lifecycle(store);

    enqueueMessages(store, QStringLiteral("session-a"), 0, 4);
    enqueueMessages(store, QStringLiteral("session-b"), 10, 4);
    QCOMPARE(store.flushPendingMessages().size(), 2);

    lifecycle.applyRetention(
        {
            SessionState {.id = QStringLiteral("session-a")},
            SessionState {.id = QStringLiteral("session-b")},
        },
        0);

    QCOMPARE(storedTopics(store, QStringLiteral("session-a")).size(), 4);
    QCOMPARE(storedTopics(store, QStringLiteral("session-b")).size(), 4);
}

void MessageRetentionLifecycleTest::exitFlushesPendingRowsBeforeRetention()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());
    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));
    MessageRetentionLifecycle lifecycle(store);

    enqueueMessages(store, QStringLiteral("session-a"), 0, 3);
    QCOMPARE(store.flushPendingMessages().size(), 1);
    enqueueMessages(store, QStringLiteral("session-a"), 3, 2);
    QCOMPARE(store.pendingMessageCount(), 2);

    lifecycle.applyExit(
        {SessionState {.id = QStringLiteral("session-a")}},
        3,
        QStringLiteral("never"),
        QStringLiteral("session-a"),
        [&store]() { store.flushPendingMessages(); });

    QCOMPARE(store.pendingMessageCount(), 0);
    QCOMPARE(storedTopics(store, QStringLiteral("session-a")),
        QStringList({QStringLiteral("topic/2"), QStringLiteral("topic/3"), QStringLiteral("topic/4")}));
}

void MessageRetentionLifecycleTest::exitAllCleanupRemainsAuthoritative()
{
    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());
    HistoryStore store(dataDir.path());
    QVERIFY2(store.isReady(), qPrintable(store.lastError()));
    MessageRetentionLifecycle lifecycle(store);

    enqueueMessages(store, QStringLiteral("session-a"), 0, 5);
    enqueueMessages(store, QStringLiteral("session-b"), 10, 5);

    lifecycle.applyExit(
        {
            SessionState {.id = QStringLiteral("session-a")},
            SessionState {.id = QStringLiteral("session-b")},
        },
        2,
        QStringLiteral("all"),
        QStringLiteral("session-a"),
        [&store]() { store.flushPendingMessages(); });

    QCOMPARE(storedTopics(store, QStringLiteral("session-a")).size(), 0);
    QCOMPARE(storedTopics(store, QStringLiteral("session-b")).size(), 0);
}

QTEST_GUILESS_MAIN(MessageRetentionLifecycleTest)

#include "test_message_retention_lifecycle.moc"
