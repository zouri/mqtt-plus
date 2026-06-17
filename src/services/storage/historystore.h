#pragma once

#include <QByteArray>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class HistoryStore
{
public:
    HistoryStore();
    ~HistoryStore();

    bool isReady() const;
    QString lastError() const;

    qint64 enqueueMessage(
        const QString &sessionId,
        const QString &timestamp,
        const QString &topic,
        const QByteArray &payloadBytes,
        const QString &parsedPayload = QString(),
        const QString &parsedFormat = QString(),
        const QString &parseError = QString(),
        const QString &scriptId = QString(),
        const QString &scriptName = QString());
    QStringList flushPendingMessages();
    int pendingMessageCount() const;
    qint64 appendEvent(
        const QString &sessionId,
        const QString &timestamp,
        const QString &channel,
        const QString &message);
    QVariantList loadMessages(const QString &sessionId, int limit) const;
    QVariantList loadMessagesBefore(const QString &sessionId, qint64 beforeId, int limit) const;
    QVariantList loadLogs(const QString &sessionId, int limit) const;
    QVariantList loadLogsBefore(const QString &sessionId, qint64 beforeId, int limit) const;
    void clearMessages(const QString &sessionId);
    void clearLogs(const QString &sessionId);
    void clearAllMessages();
    void clearAllLogs();
    void clearSessionHistory(const QString &sessionId);
    void pruneMessages(const QString &sessionId, int keepCount);
    void pruneLogs(const QString &sessionId, int keepCount);

private:
    struct PendingMessage {
        QString sessionId;
        QString timestamp;
        QString topic;
        QByteArray payloadBytes;
        QString parsedPayload;
        QString parsedFormat;
        QString parseError;
        QString scriptId;
        QString scriptName;
    };

    bool initialize();
    bool resetLegacySchema();

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_lastError;
    QVector<PendingMessage> m_pendingMessages;
    qint64 m_nextMessageId = 0;
};
