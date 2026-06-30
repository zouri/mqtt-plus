#include "app/applicationcore.h"

int ApplicationCore::messageRetentionLimit() const
{
    return m_preferencesController.messageRetentionLimit();
}

int ApplicationCore::logRetentionLimit() const
{
    return m_preferencesController.logRetentionLimit();
}

int ApplicationCore::historyPageSize() const
{
    return m_preferencesController.historyPageSize();
}

int ApplicationCore::maxIncomingPayloadBytes() const
{
    return m_preferencesController.maxIncomingPayloadBytes();
}

bool ApplicationCore::deleteHistoryWithSession() const
{
    return m_preferencesController.deleteHistoryWithSession();
}

bool ApplicationCore::saveMessagesWhenOutputPaused() const
{
    return m_preferencesController.saveMessagesWhenOutputPaused();
}

QString ApplicationCore::clearMessagesOnExit() const
{
    return m_preferencesController.clearMessagesOnExit();
}

QString ApplicationCore::clearLogsOnExit() const
{
    return m_preferencesController.clearLogsOnExit();
}

int ApplicationCore::windowWidth() const
{
    return m_preferencesController.windowWidth();
}

int ApplicationCore::windowHeight() const
{
    return m_preferencesController.windowHeight();
}

bool ApplicationCore::windowMaximized() const
{
    return m_preferencesController.windowMaximized();
}

void ApplicationCore::setMessageRetentionLimit(int limit)
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

void ApplicationCore::setLogRetentionLimit(int limit)
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

void ApplicationCore::setHistoryPageSize(int pageSize)
{
    m_preferencesController.setHistoryPageSize(pageSize);
}

void ApplicationCore::setMaxIncomingPayloadBytes(int bytes)
{
    m_preferencesController.setMaxIncomingPayloadBytes(bytes);
}

void ApplicationCore::setDeleteHistoryWithSession(bool enabled)
{
    m_preferencesController.setDeleteHistoryWithSession(enabled);
}

void ApplicationCore::setSaveMessagesWhenOutputPaused(bool enabled)
{
    m_preferencesController.setSaveMessagesWhenOutputPaused(enabled);
}

void ApplicationCore::setClearMessagesOnExit(const QString &mode)
{
    m_preferencesController.setClearMessagesOnExit(mode);
}

void ApplicationCore::setClearLogsOnExit(const QString &mode)
{
    m_preferencesController.setClearLogsOnExit(mode);
}

void ApplicationCore::setWindowMaximized(bool maximized)
{
    m_preferencesController.setWindowMaximized(maximized);
}

void ApplicationCore::saveWindowGeometry(int width, int height)
{
    m_preferencesController.setWindowGeometry(width, height);
}
