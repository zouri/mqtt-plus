#include "app/logsbus.h"

#include "controllers/eventcontroller.h"

#include <utility>

LogsBus::LogsBus(Dependencies dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(std::move(dependencies))
{
}

EventStreamModel *LogsBus::logs()
{
    return m_dependencies.logsModel;
}

int LogsBus::loadOlderCurrentSessionLogs()
{
    return m_dependencies.eventController
        ? m_dependencies.eventController->loadOlderCurrentSessionLogs()
        : 0;
}

void LogsBus::clearCurrentLogs()
{
    if (m_dependencies.eventController) {
        m_dependencies.eventController->clearCurrentLogs();
    }
}
