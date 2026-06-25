#include "app/apphistoryservices.h"

#include "controllers/eventcontroller.h"
#include "controllers/preferencescontroller.h"

#include <QtGlobal>

#include <utility>

AppHistoryServices::AppHistoryServices(Dependencies dependencies)
    : m_dependencies(std::move(dependencies))
{
    Q_ASSERT(m_dependencies.eventController);
    Q_ASSERT(m_dependencies.preferencesController);
    Q_ASSERT(m_dependencies.currentSession);
}

HistoryStore *AppHistoryServices::historyStore()
{
    return &m_historyStore;
}

const HistoryStore *AppHistoryServices::historyStore() const
{
    return &m_historyStore;
}

void AppHistoryServices::applyExitCleanup()
{
    m_dependencies.eventController->flushPendingMessageHistory();

    const auto clearMessages = [this](const QString &mode) {
        if (mode == QStringLiteral("all")) {
            m_historyStore.clearAllMessages();
        } else if (mode == QStringLiteral("current")) {
            if (auto *session = m_dependencies.currentSession()) {
                m_historyStore.clearMessages(session->id);
            }
        }
    };
    const auto clearLogs = [this](const QString &mode) {
        if (mode == QStringLiteral("all")) {
            m_historyStore.clearAllLogs();
        } else if (mode == QStringLiteral("current")) {
            if (auto *session = m_dependencies.currentSession()) {
                m_historyStore.clearLogs(session->id);
            }
        }
    };

    clearMessages(m_dependencies.preferencesController->clearMessagesOnExit());
    clearLogs(m_dependencies.preferencesController->clearLogsOnExit());
}
