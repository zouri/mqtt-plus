#include "app/logsworkspace.h"

#include "controllers/eventcontroller.h"

LogsWorkspace::LogsWorkspace(const LogsWorkspaceDependencies &dependencies)
    : m_dependencies(dependencies)
{
}

void LogsWorkspace::bindLogsSignals(QObject *context, const LogsCoreSignalHandlers &handlers)
{
    if (m_dependencies.bindLogStreamChanged && handlers.logStreamChanged) {
        m_dependencies.bindLogStreamChanged(context, handlers.logStreamChanged);
    }
    if (m_dependencies.bindLogStreamRowAppended && handlers.logStreamRowAppended) {
        m_dependencies.bindLogStreamRowAppended(context, handlers.logStreamRowAppended);
    }
}

EventStreamModel *LogsWorkspace::logs()
{
    return m_dependencies.logs;
}

void LogsWorkspace::clearCurrentLogs()
{
    if (m_dependencies.eventController) {
        m_dependencies.eventController->clearCurrentLogs();
    }
}

int LogsWorkspace::loadOlderCurrentSessionLogs()
{
    return m_dependencies.eventController
        ? m_dependencies.eventController->loadOlderCurrentSessionLogs()
        : 0;
}
