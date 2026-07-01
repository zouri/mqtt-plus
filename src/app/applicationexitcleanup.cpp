#include "app/applicationexitcleanup.h"

#include "controllers/eventcontroller.h"
#include "controllers/preferencescontroller.h"
#include "controllers/sessioncontroller.h"
#include "services/storage/historystore.h"

#include <QString>

void ApplicationExitCleanup::setDependencies(const ApplicationExitCleanupDependencies &dependencies)
{
    m_dependencies = dependencies;
}

void ApplicationExitCleanup::apply()
{
    m_dependencies.eventController->flushPendingMessageHistory();
    clearMessages(m_dependencies.preferencesController->clearMessagesOnExit());
    clearLogs(m_dependencies.preferencesController->clearLogsOnExit());
}

void ApplicationExitCleanup::clearMessages(const QString &mode)
{
    if (mode == QStringLiteral("all")) {
        m_dependencies.historyStore->clearAllMessages();
        return;
    }

    if (mode == QStringLiteral("current")) {
        if (auto *session = m_dependencies.sessionController->currentSession()) {
            m_dependencies.historyStore->clearMessages(session->id);
        }
    }
}

void ApplicationExitCleanup::clearLogs(const QString &mode)
{
    if (mode == QStringLiteral("all")) {
        m_dependencies.historyStore->clearAllLogs();
        return;
    }

    if (mode == QStringLiteral("current")) {
        if (auto *session = m_dependencies.sessionController->currentSession()) {
            m_dependencies.historyStore->clearLogs(session->id);
        }
    }
}
