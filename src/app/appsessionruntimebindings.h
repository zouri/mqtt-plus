#pragma once

#include <QObject>
#include <QTimer>

#include <functional>

struct SessionState;

class AppSessionRuntimeBindings : public QObject
{
    Q_OBJECT

public:
    struct Dependencies
    {
        std::function<void(SessionState *)> bindSessionSignals;
        std::function<void()> refreshSubscriptionFps;
    };

    explicit AppSessionRuntimeBindings(Dependencies dependencies, QObject *parent = nullptr);

    void bindSessionSignals(SessionState *session);
    bool subscriptionFpsRefreshActive() const;
    void startSubscriptionFpsRefresh();
    void setSubscriptionFpsRefreshActive(bool active);

private:
    Dependencies m_dependencies;
    QTimer m_subscriptionFpsRefreshTimer;
};
