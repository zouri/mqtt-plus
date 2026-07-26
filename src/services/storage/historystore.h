#pragma once

#include <QByteArray>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include "domain/messagerecord.h"

class HistoryStore
{
public:
    HistoryStore();
    explicit HistoryStore(const QString &dataPath);
    ~HistoryStore();

    Q_DISABLE_COPY_MOVE(HistoryStore)

    bool isReady() const;
    QString lastError() const;

    qint64 enqueueMessage(const MessageRecord &message);
    QStringList flushPendingMessages();
    int pendingMessageCount() const;
    qint64 totalMessageCount(const QString &sessionId) const;
    qint64 appendEvent(
        const QString &sessionId,
        const QString &timestamp,
        const QString &channel,
        const QString &message);
    QVariantList loadMessages(const QString &sessionId, int limit) const;
    QVariantList loadMessagesBefore(const QString &sessionId, qint64 beforeId, int limit) const;
    QVariantMap loadMessage(qint64 messageId) const;
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

private:
    bool initialize(const QString &dataPath);
    bool flushPendingMessagesForClear();
    bool executeDeletes(const QStringList &statements, const QString &sessionId = QString());

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_lastError;
    QVector<MessageRecord> m_pendingMessages;
    qint64 m_nextMessageId = 0;
};
