#include "app/appsettingsfacade.h"

#include "controllers/eventcontroller.h"
#include "controllers/languagecontroller.h"
#include "controllers/preferencescontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/themecontroller.h"
#include "models/eventstreammodel.h"
#include "services/storage/historystore.h"

#include <utility>

AppSettingsFacade::AppSettingsFacade(Dependencies dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(std::move(dependencies))
{
}

QString AppSettingsFacade::themeMode() const
{
    return m_dependencies.themeController ? m_dependencies.themeController->mode() : QString();
}

QString AppSettingsFacade::effectiveTheme() const
{
    return m_dependencies.themeController ? m_dependencies.themeController->effectiveTheme() : QString();
}

QString AppSettingsFacade::languageMode() const
{
    return m_dependencies.languageController ? m_dependencies.languageController->mode() : QString();
}

QString AppSettingsFacade::effectiveLanguage() const
{
    return m_dependencies.languageController ? m_dependencies.languageController->effectiveLanguage() : QString();
}

QVariantList AppSettingsFacade::availableLanguages() const
{
    return m_dependencies.languageController ? m_dependencies.languageController->availableLanguages() : QVariantList {};
}

int AppSettingsFacade::messageRetentionLimit() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->messageRetentionLimit() : 0;
}

int AppSettingsFacade::logRetentionLimit() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->logRetentionLimit() : 0;
}

int AppSettingsFacade::historyPageSize() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->historyPageSize() : 0;
}

int AppSettingsFacade::maxIncomingPayloadBytes() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->maxIncomingPayloadBytes() : 0;
}

bool AppSettingsFacade::deleteHistoryWithSession() const
{
    return m_dependencies.preferencesController
        && m_dependencies.preferencesController->deleteHistoryWithSession();
}

bool AppSettingsFacade::saveMessagesWhenOutputPaused() const
{
    return m_dependencies.preferencesController
        && m_dependencies.preferencesController->saveMessagesWhenOutputPaused();
}

QString AppSettingsFacade::clearMessagesOnExit() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->clearMessagesOnExit() : QString();
}

QString AppSettingsFacade::clearLogsOnExit() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->clearLogsOnExit() : QString();
}

int AppSettingsFacade::windowWidth() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->windowWidth() : 0;
}

int AppSettingsFacade::windowHeight() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->windowHeight() : 0;
}

bool AppSettingsFacade::windowMaximized() const
{
    return m_dependencies.preferencesController && m_dependencies.preferencesController->windowMaximized();
}

void AppSettingsFacade::setThemeMode(const QString &mode)
{
    if (m_dependencies.themeController) {
        m_dependencies.themeController->setMode(mode);
    }
}

void AppSettingsFacade::setLanguageMode(const QString &mode)
{
    if (m_dependencies.languageController) {
        m_dependencies.languageController->setMode(mode);
    }
}

void AppSettingsFacade::setMessageRetentionLimit(int limit)
{
    const int previousLimit = messageRetentionLimit();
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setMessageRetentionLimit(limit);
    }
    if (messageRetentionLimit() == previousLimit) {
        return;
    }
    if (messageRetentionLimit() > 0 && m_dependencies.historyStore && m_dependencies.sessionController) {
        if (m_dependencies.flushPendingMessageHistory) {
            m_dependencies.flushPendingMessageHistory();
        }
        for (const auto &session : m_dependencies.sessionController->sessions()) {
            m_dependencies.historyStore->pruneMessages(session.id, messageRetentionLimit());
        }
        if (m_dependencies.reloadCurrentSessionHistory) {
            m_dependencies.reloadCurrentSessionHistory();
        }
        if (m_dependencies.emitMessageStreamChanged) {
            m_dependencies.emitMessageStreamChanged();
        }
        if (m_dependencies.emitScriptTestSamplesChanged) {
            m_dependencies.emitScriptTestSamplesChanged();
        }
    }
}

void AppSettingsFacade::setLogRetentionLimit(int limit)
{
    const int previousLimit = logRetentionLimit();
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setLogRetentionLimit(limit);
    }
    if (logRetentionLimit() == previousLimit) {
        return;
    }
    if (logRetentionLimit() > 0 && m_dependencies.historyStore && m_dependencies.sessionController) {
        for (const auto &session : m_dependencies.sessionController->sessions()) {
            m_dependencies.historyStore->pruneLogs(session.id, logRetentionLimit());
        }
        if (m_dependencies.reloadCurrentSessionHistory) {
            m_dependencies.reloadCurrentSessionHistory();
        }
        if (m_dependencies.emitLogStreamChanged) {
            m_dependencies.emitLogStreamChanged();
        }
    }
}

void AppSettingsFacade::setHistoryPageSize(int pageSize)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setHistoryPageSize(pageSize);
    }
}

void AppSettingsFacade::setMaxIncomingPayloadBytes(int bytes)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setMaxIncomingPayloadBytes(bytes);
    }
}

void AppSettingsFacade::setDeleteHistoryWithSession(bool enabled)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setDeleteHistoryWithSession(enabled);
    }
}

void AppSettingsFacade::setSaveMessagesWhenOutputPaused(bool enabled)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setSaveMessagesWhenOutputPaused(enabled);
    }
}

void AppSettingsFacade::setClearMessagesOnExit(const QString &mode)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setClearMessagesOnExit(mode);
    }
}

void AppSettingsFacade::setClearLogsOnExit(const QString &mode)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setClearLogsOnExit(mode);
    }
}

void AppSettingsFacade::setWindowMaximized(bool maximized)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setWindowMaximized(maximized);
    }
}

void AppSettingsFacade::saveWindowGeometry(int width, int height)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setWindowGeometry(width, height);
    }
}

void AppSettingsFacade::clearAllMessages()
{
    if (!m_dependencies.historyStore || !m_dependencies.sessionController || !m_dependencies.messagesModel) {
        return;
    }
    m_dependencies.historyStore->clearAllMessages();
    for (auto &session : m_dependencies.sessionController->sessions()) {
        session.messageRows.clear();
        session.oldestLoadedMessageId = 0;
        session.loadedAllMessageHistory = true;
    }
    m_dependencies.messagesModel->clear();
    if (m_dependencies.refreshScriptTestSamplesModel) {
        m_dependencies.refreshScriptTestSamplesModel();
    }
    if (m_dependencies.emitMessageStreamChanged) {
        m_dependencies.emitMessageStreamChanged();
    }
    if (m_dependencies.emitScriptTestSamplesChanged) {
        m_dependencies.emitScriptTestSamplesChanged();
    }
}

void AppSettingsFacade::clearAllLogs()
{
    if (!m_dependencies.historyStore || !m_dependencies.sessionController || !m_dependencies.logsModel) {
        return;
    }
    m_dependencies.historyStore->clearAllLogs();
    for (auto &session : m_dependencies.sessionController->sessions()) {
        session.logRows.clear();
        session.oldestLoadedLogId = 0;
        session.loadedAllLogHistory = true;
    }
    m_dependencies.logsModel->clear();
    if (m_dependencies.emitLogStreamChanged) {
        m_dependencies.emitLogStreamChanged();
    }
}

void AppSettingsFacade::clearAllHistory()
{
    if (!m_dependencies.historyStore || !m_dependencies.sessionController
        || !m_dependencies.messagesModel || !m_dependencies.logsModel) {
        return;
    }
    m_dependencies.historyStore->clearAllMessages();
    m_dependencies.historyStore->clearAllLogs();
    for (auto &session : m_dependencies.sessionController->sessions()) {
        session.messageRows.clear();
        session.oldestLoadedMessageId = 0;
        session.loadedAllMessageHistory = true;
        session.logRows.clear();
        session.oldestLoadedLogId = 0;
        session.loadedAllLogHistory = true;
    }
    m_dependencies.messagesModel->clear();
    m_dependencies.logsModel->clear();
    if (m_dependencies.refreshScriptTestSamplesModel) {
        m_dependencies.refreshScriptTestSamplesModel();
    }
    if (m_dependencies.emitMessageStreamChanged) {
        m_dependencies.emitMessageStreamChanged();
    }
    if (m_dependencies.emitLogStreamChanged) {
        m_dependencies.emitLogStreamChanged();
    }
    if (m_dependencies.emitScriptTestSamplesChanged) {
        m_dependencies.emitScriptTestSamplesChanged();
    }
}
