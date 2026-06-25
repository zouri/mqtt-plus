#pragma once

#include <QObject>

#include "app/connectionbus.h"
#include "app/logsbus.h"
#include "app/messagesbus.h"
#include "app/scriptsbus.h"
#include "app/sessionsbus.h"
#include "app/settingsbus.h"
#include "app/subscriptionsbus.h"

class AppRuntime;

class AppBus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SessionsBus* sessions READ sessions CONSTANT)
    Q_PROPERTY(SubscriptionsBus* subscriptions READ subscriptions CONSTANT)
    Q_PROPERTY(ConnectionBus* connection READ connection CONSTANT)
    Q_PROPERTY(MessagesBus* messages READ messages CONSTANT)
    Q_PROPERTY(SettingsBus* settings READ settings CONSTANT)
    Q_PROPERTY(ScriptsBus* scripts READ scripts CONSTANT)
    Q_PROPERTY(LogsBus* logs READ logs CONSTANT)

public:
    explicit AppBus(AppRuntime *runtime, QObject *parent = nullptr);

    SessionsBus *sessions();
    SubscriptionsBus *subscriptions();
    ConnectionBus *connection();
    MessagesBus *messages();
    SettingsBus *settings();
    ScriptsBus *scripts();
    LogsBus *logs();

private:
    AppRuntime *m_runtime = nullptr;
};
