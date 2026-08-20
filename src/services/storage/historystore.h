#pragma once

#include <QByteArray>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

#include <optional>

#include "domain/messagerecord.h"
#include "domain/messageparsing.h"
#include "domain/topicobservation.h"

struct HistoryWriteResult
{
    bool ok = false;
    QStringList sessionIds;
    int messageCount = 0;
    QString error;
};

class HistoryStore
{
public:
    HistoryStore();
    explicit HistoryStore(const QString &dataPath);
    HistoryStore(const QString &dataPath, int busyTimeoutMs);
    ~HistoryStore();

    Q_DISABLE_COPY_MOVE(HistoryStore)

    bool isReady() const;
    QString lastError() const;
    void clearLastError();
    QString dataPath() const;
    QString journalMode() const;
    int busyTimeoutMs() const;
    qint64 nextMessageId();
    HistoryWriteResult appendMessages(const QVector<MessageRecord> &messages);
    HistoryWriteResult writeMessageBatch(
        const QVector<MessageRecord> &messages,
        const QVector<ParseOutcome> &parseResults);

    qint64 totalMessageCount(const QString &sessionId) const;
    qint64 appendEvent(
        const QString &sessionId,
        const QString &timestamp,
        const QString &channel,
        const QString &message);
    QVector<MessageRecord> loadMessages(const QString &sessionId, int limit) const;
    QVector<MessageRecord> loadMessagesBefore(
        const QString &sessionId,
        qint64 beforeId,
        int limit) const;
    QVector<TopicObservation> loadLatestIncomingTopics(
        const QString &sessionId,
        int limit) const;
    std::optional<MessageRecord> loadMessage(qint64 messageId) const;
    QByteArray loadMessagePayloadBytes(qint64 messageId) const;
    QVariantList loadLogs(const QString &sessionId, int limit) const;
    QVariantList loadLogsBefore(const QString &sessionId, qint64 beforeId, int limit) const;
    bool clearMessages(const QString &sessionId);
    bool clearLogs(const QString &sessionId);
    bool clearAllMessages();
    bool clearAllLogs();
    bool clearSessionHistory(const QString &sessionId);
    bool clearAllHistory();
    void pruneMessages(const QString &sessionId, int keepCount);
    void pruneLogs(const QString &sessionId, int keepCount);
    bool reclaimFreePages();

private:
    bool initialize(const QString &dataPath, int busyTimeoutMs);
    bool executeDeletes(const QStringList &statements, const QString &sessionId = QString());

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_lastError;
    QString m_dataPath;
};
