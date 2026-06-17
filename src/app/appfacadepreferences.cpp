#include "app/appfacade.h"

int AppFacade::messageRetentionLimit() const
{
    return m_preferencesController.messageRetentionLimit();
}

int AppFacade::logRetentionLimit() const
{
    return m_preferencesController.logRetentionLimit();
}

int AppFacade::historyPageSize() const
{
    return m_preferencesController.historyPageSize();
}

bool AppFacade::deleteHistoryWithSession() const
{
    return m_preferencesController.deleteHistoryWithSession();
}

bool AppFacade::saveMessagesWhenOutputPaused() const
{
    return m_preferencesController.saveMessagesWhenOutputPaused();
}

QString AppFacade::clearMessagesOnExit() const
{
    return m_preferencesController.clearMessagesOnExit();
}

QString AppFacade::clearLogsOnExit() const
{
    return m_preferencesController.clearLogsOnExit();
}

void AppFacade::setMessageRetentionLimit(int limit)
{
    const int previousLimit = messageRetentionLimit();
    m_preferencesController.setMessageRetentionLimit(limit);
    if (messageRetentionLimit() == previousLimit) {
        return;
    }
    if (messageRetentionLimit() > 0) {
        m_eventController.flushPendingMessageHistory();
        for (const auto &session : m_sessionController.sessions()) {
            m_historyStore.pruneMessages(session.id, messageRetentionLimit());
        }
        reloadCurrentSessionHistory();
        emit messageStreamChanged();
        emit scriptTestSamplesChanged();
    }
}

void AppFacade::setLogRetentionLimit(int limit)
{
    const int previousLimit = logRetentionLimit();
    m_preferencesController.setLogRetentionLimit(limit);
    if (logRetentionLimit() == previousLimit) {
        return;
    }
    if (logRetentionLimit() > 0) {
        for (const auto &session : m_sessionController.sessions()) {
            m_historyStore.pruneLogs(session.id, logRetentionLimit());
        }
        reloadCurrentSessionHistory();
        emit logStreamChanged();
    }
}

void AppFacade::setHistoryPageSize(int pageSize)
{
    m_preferencesController.setHistoryPageSize(pageSize);
}

void AppFacade::setDeleteHistoryWithSession(bool enabled)
{
    m_preferencesController.setDeleteHistoryWithSession(enabled);
}

void AppFacade::setSaveMessagesWhenOutputPaused(bool enabled)
{
    m_preferencesController.setSaveMessagesWhenOutputPaused(enabled);
}

void AppFacade::setClearMessagesOnExit(const QString &mode)
{
    m_preferencesController.setClearMessagesOnExit(mode);
}

void AppFacade::setClearLogsOnExit(const QString &mode)
{
    m_preferencesController.setClearLogsOnExit(mode);
}
