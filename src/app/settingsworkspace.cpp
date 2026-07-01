#include "app/settingsworkspace.h"

#include "controllers/eventcontroller.h"
#include "controllers/languagecontroller.h"
#include "controllers/preferencescontroller.h"
#include "controllers/themecontroller.h"
#include "models/eventstreammodel.h"
#include "services/storage/historystore.h"

SettingsWorkspace::SettingsWorkspace(const SettingsWorkspaceDependencies &dependencies)
    : m_dependencies(dependencies)
{
}

void SettingsWorkspace::bindSettingsSignals(QObject *context, const SettingsCoreSignalHandlers &handlers)
{
    if (m_dependencies.bindThemeModeChanged && handlers.themeModeChanged) {
        m_dependencies.bindThemeModeChanged(context, handlers.themeModeChanged);
    }
    if (m_dependencies.bindEffectiveThemeChanged && handlers.effectiveThemeChanged) {
        m_dependencies.bindEffectiveThemeChanged(context, handlers.effectiveThemeChanged);
    }
    if (m_dependencies.bindLanguageModeChanged && handlers.languageModeChanged) {
        m_dependencies.bindLanguageModeChanged(context, handlers.languageModeChanged);
    }
    if (m_dependencies.bindLanguageChanged && handlers.languageChanged) {
        m_dependencies.bindLanguageChanged(context, handlers.languageChanged);
    }
    if (m_dependencies.bindMessageRetentionLimitChanged && handlers.messageRetentionLimitChanged) {
        m_dependencies.bindMessageRetentionLimitChanged(context, handlers.messageRetentionLimitChanged);
    }
    if (m_dependencies.bindLogRetentionLimitChanged && handlers.logRetentionLimitChanged) {
        m_dependencies.bindLogRetentionLimitChanged(context, handlers.logRetentionLimitChanged);
    }
    if (m_dependencies.bindHistoryPageSizeChanged && handlers.historyPageSizeChanged) {
        m_dependencies.bindHistoryPageSizeChanged(context, handlers.historyPageSizeChanged);
    }
    if (m_dependencies.bindMaxIncomingPayloadBytesChanged && handlers.maxIncomingPayloadBytesChanged) {
        m_dependencies.bindMaxIncomingPayloadBytesChanged(context, handlers.maxIncomingPayloadBytesChanged);
    }
    if (m_dependencies.bindDeleteHistoryWithSessionChanged && handlers.deleteHistoryWithSessionChanged) {
        m_dependencies.bindDeleteHistoryWithSessionChanged(context, handlers.deleteHistoryWithSessionChanged);
    }
    if (m_dependencies.bindSaveMessagesWhenOutputPausedChanged && handlers.saveMessagesWhenOutputPausedChanged) {
        m_dependencies.bindSaveMessagesWhenOutputPausedChanged(context, handlers.saveMessagesWhenOutputPausedChanged);
    }
    if (m_dependencies.bindClearMessagesOnExitChanged && handlers.clearMessagesOnExitChanged) {
        m_dependencies.bindClearMessagesOnExitChanged(context, handlers.clearMessagesOnExitChanged);
    }
    if (m_dependencies.bindClearLogsOnExitChanged && handlers.clearLogsOnExitChanged) {
        m_dependencies.bindClearLogsOnExitChanged(context, handlers.clearLogsOnExitChanged);
    }
    if (m_dependencies.bindWindowWidthChanged && handlers.windowWidthChanged) {
        m_dependencies.bindWindowWidthChanged(context, handlers.windowWidthChanged);
    }
    if (m_dependencies.bindWindowHeightChanged && handlers.windowHeightChanged) {
        m_dependencies.bindWindowHeightChanged(context, handlers.windowHeightChanged);
    }
    if (m_dependencies.bindWindowMaximizedChanged && handlers.windowMaximizedChanged) {
        m_dependencies.bindWindowMaximizedChanged(context, handlers.windowMaximizedChanged);
    }
}

QString SettingsWorkspace::themeMode() const
{
    return m_dependencies.themeController ? m_dependencies.themeController->mode() : QStringLiteral("system");
}

QString SettingsWorkspace::effectiveTheme() const
{
    return m_dependencies.themeController ? m_dependencies.themeController->effectiveTheme() : QStringLiteral("light");
}

QString SettingsWorkspace::languageMode() const
{
    return m_dependencies.languageController ? m_dependencies.languageController->mode() : QStringLiteral("system");
}

int SettingsWorkspace::messageRetentionLimit() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->messageRetentionLimit() : 5000;
}

int SettingsWorkspace::logRetentionLimit() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->logRetentionLimit() : 2000;
}

int SettingsWorkspace::historyPageSize() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->historyPageSize() : 500;
}

int SettingsWorkspace::maxIncomingPayloadBytes() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->maxIncomingPayloadBytes() : 1024 * 1024;
}

bool SettingsWorkspace::deleteHistoryWithSession() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->deleteHistoryWithSession() : true;
}

bool SettingsWorkspace::saveMessagesWhenOutputPaused() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->saveMessagesWhenOutputPaused() : true;
}

QString SettingsWorkspace::clearMessagesOnExit() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->clearMessagesOnExit() : QStringLiteral("never");
}

QString SettingsWorkspace::clearLogsOnExit() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->clearLogsOnExit() : QStringLiteral("never");
}

int SettingsWorkspace::windowWidth() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->windowWidth() : 1480;
}

int SettingsWorkspace::windowHeight() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->windowHeight() : 820;
}

bool SettingsWorkspace::windowMaximized() const
{
    return m_dependencies.preferencesController && m_dependencies.preferencesController->windowMaximized();
}

void SettingsWorkspace::setThemeMode(const QString &mode)
{
    if (m_dependencies.themeController) {
        m_dependencies.themeController->setMode(mode);
    }
}

void SettingsWorkspace::setLanguageMode(const QString &mode)
{
    if (m_dependencies.languageController) {
        m_dependencies.languageController->setMode(mode);
    }
}

void SettingsWorkspace::setMessageRetentionLimit(int limit)
{
    if (!m_dependencies.preferencesController) {
        return;
    }

    const int previousLimit = messageRetentionLimit();
    m_dependencies.preferencesController->setMessageRetentionLimit(limit);
    if (messageRetentionLimit() == previousLimit || messageRetentionLimit() <= 0) {
        return;
    }

    if (m_dependencies.eventController) {
        m_dependencies.eventController->flushPendingMessageHistory();
    }
    if (m_dependencies.historyStore && m_dependencies.sessions) {
        for (const auto &session : *m_dependencies.sessions) {
            m_dependencies.historyStore->pruneMessages(session.id, messageRetentionLimit());
        }
    }
    if (m_dependencies.reloadCurrentSessionHistory) {
        m_dependencies.reloadCurrentSessionHistory();
    }
    if (m_dependencies.emitMessageStreamChanged) {
        m_dependencies.emitMessageStreamChanged();
    }
}

void SettingsWorkspace::setLogRetentionLimit(int limit)
{
    if (!m_dependencies.preferencesController) {
        return;
    }

    const int previousLimit = logRetentionLimit();
    m_dependencies.preferencesController->setLogRetentionLimit(limit);
    if (logRetentionLimit() == previousLimit || logRetentionLimit() <= 0) {
        return;
    }

    if (m_dependencies.historyStore && m_dependencies.sessions) {
        for (const auto &session : *m_dependencies.sessions) {
            m_dependencies.historyStore->pruneLogs(session.id, logRetentionLimit());
        }
    }
    if (m_dependencies.reloadCurrentSessionHistory) {
        m_dependencies.reloadCurrentSessionHistory();
    }
    if (m_dependencies.emitLogStreamChanged) {
        m_dependencies.emitLogStreamChanged();
    }
}

void SettingsWorkspace::setHistoryPageSize(int pageSize)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setHistoryPageSize(pageSize);
    }
}

void SettingsWorkspace::setMaxIncomingPayloadBytes(int bytes)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setMaxIncomingPayloadBytes(bytes);
    }
}

void SettingsWorkspace::setDeleteHistoryWithSession(bool enabled)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setDeleteHistoryWithSession(enabled);
    }
}

void SettingsWorkspace::setSaveMessagesWhenOutputPaused(bool enabled)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setSaveMessagesWhenOutputPaused(enabled);
    }
}

void SettingsWorkspace::setClearMessagesOnExit(const QString &mode)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setClearMessagesOnExit(mode);
    }
}

void SettingsWorkspace::setClearLogsOnExit(const QString &mode)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setClearLogsOnExit(mode);
    }
}

void SettingsWorkspace::setWindowMaximized(bool maximized)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setWindowMaximized(maximized);
    }
}

void SettingsWorkspace::saveWindowGeometry(int width, int height)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setWindowGeometry(width, height);
    }
}

void SettingsWorkspace::clearAllMessages()
{
    if (m_dependencies.historyStore) {
        m_dependencies.historyStore->clearAllMessages();
    }
    if (m_dependencies.sessions) {
        for (auto &session : *m_dependencies.sessions) {
            session.messageRows.clear();
            session.oldestLoadedMessageId = 0;
            session.loadedAllMessageHistory = true;
        }
    }
    if (m_dependencies.messages) {
        m_dependencies.messages->clear();
    }
    if (m_dependencies.refreshScriptTestSamplesModel) {
        m_dependencies.refreshScriptTestSamplesModel();
    }
    if (m_dependencies.emitMessageStreamChanged) {
        m_dependencies.emitMessageStreamChanged();
    }
}

void SettingsWorkspace::clearAllLogs()
{
    if (m_dependencies.historyStore) {
        m_dependencies.historyStore->clearAllLogs();
    }
    if (m_dependencies.sessions) {
        for (auto &session : *m_dependencies.sessions) {
            session.logRows.clear();
            session.oldestLoadedLogId = 0;
            session.loadedAllLogHistory = true;
        }
    }
    if (m_dependencies.logs) {
        m_dependencies.logs->clear();
    }
    if (m_dependencies.emitLogStreamChanged) {
        m_dependencies.emitLogStreamChanged();
    }
}

void SettingsWorkspace::clearAllHistory()
{
    if (m_dependencies.historyStore) {
        m_dependencies.historyStore->clearAllMessages();
        m_dependencies.historyStore->clearAllLogs();
    }
    if (m_dependencies.sessions) {
        for (auto &session : *m_dependencies.sessions) {
            session.messageRows.clear();
            session.oldestLoadedMessageId = 0;
            session.loadedAllMessageHistory = true;
            session.logRows.clear();
            session.oldestLoadedLogId = 0;
            session.loadedAllLogHistory = true;
        }
    }
    if (m_dependencies.messages) {
        m_dependencies.messages->clear();
    }
    if (m_dependencies.logs) {
        m_dependencies.logs->clear();
    }
    if (m_dependencies.refreshScriptTestSamplesModel) {
        m_dependencies.refreshScriptTestSamplesModel();
    }
    if (m_dependencies.emitMessageStreamChanged) {
        m_dependencies.emitMessageStreamChanged();
    }
    if (m_dependencies.emitLogStreamChanged) {
        m_dependencies.emitLogStreamChanged();
    }
}
