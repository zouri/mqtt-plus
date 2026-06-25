#include "app/appbus.h"

#include "app/appruntimestate.h"
#include "app/appruntime.h"

AppBus::AppBus(AppRuntime *runtime, QObject *parent)
    : QObject(parent)
    , m_runtime(runtime)
{
}

SessionsBus *AppBus::sessions()
{
    return m_runtime->sessions();
}

SubscriptionsBus *AppBus::subscriptions()
{
    return m_runtime->subscriptions();
}

ConnectionBus *AppBus::connection()
{
    return m_runtime->connection();
}

MessagesBus *AppBus::messages()
{
    return m_runtime->messages();
}

SettingsBus *AppBus::settings()
{
    return m_runtime->settings();
}

ScriptsBus *AppBus::scripts()
{
    return m_runtime->scripts();
}

LogsBus *AppBus::logs()
{
    return m_runtime->logs();
}
