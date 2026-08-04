#pragma once

#include <QObject>
#include <QHash>
#include <QSslConfiguration>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "domain/session.h"

class EventHistoryService;
class SessionService;
class SubscriptionService;

class MqttSessionService : public QObject
{
    Q_OBJECT

public:
    explicit MqttSessionService(
        SessionService &sessionService,
        SubscriptionService &subscriptionService,
        EventHistoryService &eventHistoryService,
        QObject *parent = nullptr);

    void connectCurrentSession();
    void disconnectCurrentSession();
    bool publishCurrentSession(
        const QString &topic,
        const QString &payload,
        int format,
        int qos,
        bool retain,
        const QString &sourceLabel = QString());
    QVariantList recentPublishes() const;
    void clearRecentPublishes();
    void bindSessionSignals(SessionState *session);
    void connectSession(SessionState &session, const QString &eventPrefix);

signals:
    void sessionStateChanged();
    void publishProgress(const QVariantMap &status);
    void recentPublishesChanged();

private:
    QSslConfiguration sslConfigurationForSession(
        const SessionState &session,
        QString &errorMessage) const;
    void updatePublishStatus(
        SessionState &session,
        const QString &state,
        const QString &reason = QString(),
        qint32 messageId = -1);
    void emitPublishProgress(const PublishStatus &status);
    void finishPendingPublishes(const QString &sessionId, const QString &reason);
    void recordRecentPublish(
        const QString &topic,
        const QString &payload,
        int format,
        int qos,
        bool retain,
        qint64 encodedSize);
    static QString pendingKey(const QString &sessionId, qint32 messageId);

    SessionService &m_sessionService;
    SubscriptionService &m_subscriptionService;
    EventHistoryService &m_eventHistoryService;
    QHash<QString, PublishStatus> m_pendingPublishes;
    QVariantList m_recentPublishes;
    qint64 m_recentPublishBytes = 0;
};
