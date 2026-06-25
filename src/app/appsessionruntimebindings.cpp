#include "app/appsessionruntimebindings.h"

#include "app/appruntimeutils.h"

#include <QtGlobal>

#include <utility>

AppSessionRuntimeBindings::AppSessionRuntimeBindings(Dependencies dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(std::move(dependencies))
{
    Q_ASSERT(m_dependencies.bindSessionSignals);
    Q_ASSERT(m_dependencies.refreshSubscriptionFps);

    m_subscriptionFpsRefreshTimer.setInterval(AppRuntimeUtils::kSubscriptionFpsRefreshIntervalMs);
    connect(
        &m_subscriptionFpsRefreshTimer,
        &QTimer::timeout,
        this,
        [this]() {
            m_dependencies.refreshSubscriptionFps();
        });
}

void AppSessionRuntimeBindings::bindSessionSignals(SessionState *session)
{
    m_dependencies.bindSessionSignals(session);
}

bool AppSessionRuntimeBindings::subscriptionFpsRefreshActive() const
{
    return m_subscriptionFpsRefreshTimer.isActive();
}

void AppSessionRuntimeBindings::startSubscriptionFpsRefresh()
{
    m_subscriptionFpsRefreshTimer.start();
}

void AppSessionRuntimeBindings::setSubscriptionFpsRefreshActive(bool active)
{
    if (active) {
        m_subscriptionFpsRefreshTimer.start();
    } else {
        m_subscriptionFpsRefreshTimer.stop();
    }
}
