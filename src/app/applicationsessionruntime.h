#pragma once

#include "domain/session.h"

#include <QObject>
#include <QString>

#include <functional>

struct ApplicationSessionRuntimeCallbacks
{
    std::function<SessionState *(const QString &)> sessionById;
    std::function<void(SessionState &, const QString &, const QString &)> appendEvent;
    std::function<void()> notifySessionViewsChanged;
    std::function<void(SessionState *)> bindSessionSignals;
};

class ApplicationSessionRuntime
{
public:
    ApplicationSessionRuntime(QObject *owner, ApplicationSessionRuntimeCallbacks callbacks);

    void initialize(SessionState *session);
    void destroy(SessionState &session);
    void bindSignals(SessionState *session);
    SessionState createDefaultSession(const QString &name);

private:
    QObject *m_owner = nullptr;
    ApplicationSessionRuntimeCallbacks m_callbacks;
};
