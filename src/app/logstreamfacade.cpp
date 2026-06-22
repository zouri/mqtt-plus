#include "app/logstreamfacade.h"

#include "controllers/eventcontroller.h"

#include <utility>

LogStreamFacade::LogStreamFacade(Dependencies dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(std::move(dependencies))
{
}

EventStreamModel *LogStreamFacade::logs()
{
    return m_dependencies.logsModel;
}

int LogStreamFacade::loadOlderCurrentSessionLogs()
{
    return m_dependencies.eventController
        ? m_dependencies.eventController->loadOlderCurrentSessionLogs()
        : 0;
}

void LogStreamFacade::clearCurrentLogs()
{
    if (m_dependencies.eventController) {
        m_dependencies.eventController->clearCurrentLogs();
    }
}
