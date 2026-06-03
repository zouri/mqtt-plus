#pragma once

#include <QByteArray>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>

class HistoryStore
{
public:
    HistoryStore();
    ~HistoryStore();

    bool isReady() const;
    QString lastError() const;

    qint64 appendMessage(
        const QString &sessionId,
        const QString &timestamp,
        const QString &topic,
        const QByteArray &payloadBytes,
        const QString &parsedPayload = QString(),
        const QString &parsedFormat = QString(),
        const QString &parseError = QString(),
        const QString &scriptId = QString(),
        const QString &scriptName = QString());
    qint64 appendEvent(
        const QString &sessionId,
        const QString &timestamp,
        const QString &channel,
        const QString &message);
    QVariantList loadEntries(const QString &sessionId, int limit) const;
    QVariantList loadEntriesBefore(const QString &sessionId, qint64 beforeId, int limit) const;
    void clearMessages(const QString &sessionId);

private:
    bool initialize();

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_lastError;
};
