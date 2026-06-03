#pragma once

#include "domain/session.h"
#include "domain/subscription.h"
#include "services/scripting/luarunner.h"

#include <QObject>
#include <QVariantMap>

class AppFacade;

class EventController : public QObject
{
    Q_OBJECT

public:
    explicit EventController(AppFacade *app, QObject *parent = nullptr);

    void clearCurrentMessages();
    int loadOlderCurrentSessionEvents();
    void appendRenderedEventRow(SessionState &session, const QVariantMap &row);
    void appendEvent(SessionState &session, const QString &channel, const QString &message);
    LuaScriptResult parseIncomingPayload(
        const SessionState &session,
        const SubscriptionEntry *subscription,
        const QString &topic,
        const QByteArray &payloadBytes,
        const QString &timestamp,
        QString &scriptNameOut,
        QString &decodedPayloadOut) const;
    void appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes);
    void trimVisibleEventRows(SessionState &session);
    void reloadCurrentSessionHistory();

private:
    AppFacade &m_app;
};
