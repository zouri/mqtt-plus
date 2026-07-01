#pragma once

#include "viewmodels/logscoreport.h"

#include <QObject>
#include <QVariantMap>

#include <functional>

class EventController;
class EventStreamModel;

struct LogsWorkspaceDependencies {
    EventStreamModel *logs = nullptr;
    EventController *eventController = nullptr;
    std::function<void(QObject *, std::function<void()>)> bindLogStreamChanged;
    std::function<void(QObject *, std::function<void(const QVariantMap &)>)> bindLogStreamRowAppended;
};

class LogsWorkspace : public LogsCorePort
{
public:
    explicit LogsWorkspace(const LogsWorkspaceDependencies &dependencies = {});

    void bindLogsSignals(QObject *context, const LogsCoreSignalHandlers &handlers) override;
    EventStreamModel *logs() override;
    void clearCurrentLogs() override;
    int loadOlderCurrentSessionLogs() override;

private:
    LogsWorkspaceDependencies m_dependencies;
};
