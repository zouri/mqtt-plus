#include "app/settingsbus.h"

#include "controllers/eventcontroller.h"
#include "controllers/languagecontroller.h"
#include "controllers/preferencescontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/themecontroller.h"
#include "models/eventstreammodel.h"
#include "services/storage/historystore.h"

#include <utility>

SettingsBus::SettingsBus(Dependencies dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(std::move(dependencies))
{
}

QString SettingsBus::themeMode() const
{
    return m_dependencies.themeController ? m_dependencies.themeController->mode() : QString();
}

QString SettingsBus::effectiveTheme() const
{
    return m_dependencies.themeController ? m_dependencies.themeController->effectiveTheme() : QString();
}

QString SettingsBus::languageMode() const
{
    return m_dependencies.languageController ? m_dependencies.languageController->mode() : QString();
}

QString SettingsBus::effectiveLanguage() const
{
    return m_dependencies.languageController ? m_dependencies.languageController->effectiveLanguage() : QString();
}

QVariantList SettingsBus::availableLanguages() const
{
    return m_dependencies.languageController ? m_dependencies.languageController->availableLanguages() : QVariantList {};
}

int SettingsBus::messageRetentionLimit() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->messageRetentionLimit() : 0;
}

int SettingsBus::logRetentionLimit() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->logRetentionLimit() : 0;
}

int SettingsBus::historyPageSize() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->historyPageSize() : 0;
}

bool SettingsBus::deleteHistoryWithSession() const
{
    return m_dependencies.preferencesController
        && m_dependencies.preferencesController->deleteHistoryWithSession();
}

bool SettingsBus::saveMessagesWhenOutputPaused() const
{
    return m_dependencies.preferencesController
        && m_dependencies.preferencesController->saveMessagesWhenOutputPaused();
}

QString SettingsBus::clearMessagesOnExit() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->clearMessagesOnExit() : QString();
}

QString SettingsBus::clearLogsOnExit() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->clearLogsOnExit() : QString();
}

int SettingsBus::windowWidth() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->windowWidth() : 0;
}

int SettingsBus::windowHeight() const
{
    return m_dependencies.preferencesController ? m_dependencies.preferencesController->windowHeight() : 0;
}

bool SettingsBus::windowMaximized() const
{
    return m_dependencies.preferencesController && m_dependencies.preferencesController->windowMaximized();
}

void SettingsBus::setThemeMode(const QString &mode)
{
    if (m_dependencies.themeController) {
        m_dependencies.themeController->setMode(mode);
    }
}

void SettingsBus::setLanguageMode(const QString &mode)
{
    if (m_dependencies.languageController) {
        m_dependencies.languageController->setMode(mode);
    }
}

void SettingsBus::setMessageRetentionLimit(int limit)
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
        if (m_dependencies.publishMessageStreamChanged) {
            m_dependencies.publishMessageStreamChanged();
        }
        if (m_dependencies.publishScriptTestSamplesChanged) {
            m_dependencies.publishScriptTestSamplesChanged();
        }
    }
}

void SettingsBus::setLogRetentionLimit(int limit)
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
        if (m_dependencies.publishLogsChanged) {
            m_dependencies.publishLogsChanged();
        }
    }
}

void SettingsBus::setHistoryPageSize(int pageSize)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setHistoryPageSize(pageSize);
    }
}

void SettingsBus::setDeleteHistoryWithSession(bool enabled)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setDeleteHistoryWithSession(enabled);
    }
}

void SettingsBus::setSaveMessagesWhenOutputPaused(bool enabled)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setSaveMessagesWhenOutputPaused(enabled);
    }
}

void SettingsBus::setClearMessagesOnExit(const QString &mode)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setClearMessagesOnExit(mode);
    }
}

void SettingsBus::setClearLogsOnExit(const QString &mode)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setClearLogsOnExit(mode);
    }
}

void SettingsBus::setWindowMaximized(bool maximized)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setWindowMaximized(maximized);
    }
}

void SettingsBus::saveWindowGeometry(int width, int height)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setWindowGeometry(width, height);
    }
}

void SettingsBus::clearAllMessages()
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
    if (m_dependencies.syncScriptTestSamplesModel) {
        m_dependencies.syncScriptTestSamplesModel();
    }
    if (m_dependencies.publishMessageStreamChanged) {
        m_dependencies.publishMessageStreamChanged();
    }
    if (m_dependencies.publishScriptTestSamplesChanged) {
        m_dependencies.publishScriptTestSamplesChanged();
    }
}

void SettingsBus::clearAllLogs()
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
    if (m_dependencies.publishLogsChanged) {
        m_dependencies.publishLogsChanged();
    }
}

void SettingsBus::clearAllHistory()
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
    if (m_dependencies.syncScriptTestSamplesModel) {
        m_dependencies.syncScriptTestSamplesModel();
    }
    if (m_dependencies.publishMessageStreamChanged) {
        m_dependencies.publishMessageStreamChanged();
    }
    if (m_dependencies.publishLogsChanged) {
        m_dependencies.publishLogsChanged();
    }
    if (m_dependencies.publishScriptTestSamplesChanged) {
        m_dependencies.publishScriptTestSamplesChanged();
    }
}
