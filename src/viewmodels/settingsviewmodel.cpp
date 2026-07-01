#include "viewmodels/settingsviewmodel.h"

#include "controllers/eventcontroller.h"
#include "controllers/languagecontroller.h"
#include "controllers/preferencescontroller.h"
#include "controllers/themecontroller.h"
#include "models/eventstreammodel.h"
#include "services/storage/historystore.h"

namespace {
QVariantList themeModeValues()
{
    return {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")};
}

QVariantList languageModeValues()
{
    return {QStringLiteral("system"), QStringLiteral("en"), QStringLiteral("zh_CN")};
}

QVariantList messageRetentionLimitValues()
{
    return {1000, 5000, 10000, 0};
}

QVariantList logRetentionLimitValues()
{
    return {500, 2000, 5000, 0};
}

QVariantList historyPageSizeValues()
{
    return {200, 500, 1000};
}

QVariantList maxIncomingPayloadByteValues()
{
    return {262144, 1048576, 5242880, 16777216};
}

QVariantList cleanupModeValues()
{
    return {QStringLiteral("never"), QStringLiteral("current"), QStringLiteral("all")};
}
}

SettingsViewModel::SettingsViewModel(QObject *parent)
    : SettingsViewModel(Dependencies {}, parent)
{
}

SettingsViewModel::SettingsViewModel(const Dependencies &dependencies, QObject *parent)
    : SettingsOptionsViewModel(parent)
    , m_dependencies(dependencies)
{
    if (m_dependencies.bindThemeModeChanged) {
        m_dependencies.bindThemeModeChanged(this, [this]() { emit themeModeChanged(); });
    }
    if (m_dependencies.bindEffectiveThemeChanged) {
        m_dependencies.bindEffectiveThemeChanged(this, [this]() { emit effectiveThemeChanged(); });
    }
    if (m_dependencies.bindLanguageModeChanged) {
        m_dependencies.bindLanguageModeChanged(this, [this]() { emit languageModeChanged(); });
    }
    if (m_dependencies.bindLanguageChanged) {
        m_dependencies.bindLanguageChanged(this, [this]() { emit languageChanged(); });
    }
    if (m_dependencies.bindMessageRetentionLimitChanged) {
        m_dependencies.bindMessageRetentionLimitChanged(this, [this]() { emit messageRetentionLimitChanged(); });
    }
    if (m_dependencies.bindLogRetentionLimitChanged) {
        m_dependencies.bindLogRetentionLimitChanged(this, [this]() { emit logRetentionLimitChanged(); });
    }
    if (m_dependencies.bindHistoryPageSizeChanged) {
        m_dependencies.bindHistoryPageSizeChanged(this, [this]() { emit historyPageSizeChanged(); });
    }
    if (m_dependencies.bindMaxIncomingPayloadBytesChanged) {
        m_dependencies.bindMaxIncomingPayloadBytesChanged(this, [this]() { emit maxIncomingPayloadBytesChanged(); });
    }
    if (m_dependencies.bindDeleteHistoryWithSessionChanged) {
        m_dependencies.bindDeleteHistoryWithSessionChanged(this, [this]() { emit deleteHistoryWithSessionChanged(); });
    }
    if (m_dependencies.bindSaveMessagesWhenOutputPausedChanged) {
        m_dependencies.bindSaveMessagesWhenOutputPausedChanged(this, [this]() { emit saveMessagesWhenOutputPausedChanged(); });
    }
    if (m_dependencies.bindClearMessagesOnExitChanged) {
        m_dependencies.bindClearMessagesOnExitChanged(this, [this]() { emit clearMessagesOnExitChanged(); });
    }
    if (m_dependencies.bindClearLogsOnExitChanged) {
        m_dependencies.bindClearLogsOnExitChanged(this, [this]() { emit clearLogsOnExitChanged(); });
    }
    if (m_dependencies.bindWindowWidthChanged) {
        m_dependencies.bindWindowWidthChanged(this, [this]() { emit windowWidthChanged(); });
    }
    if (m_dependencies.bindWindowHeightChanged) {
        m_dependencies.bindWindowHeightChanged(this, [this]() { emit windowHeightChanged(); });
    }
    if (m_dependencies.bindWindowMaximizedChanged) {
        m_dependencies.bindWindowMaximizedChanged(this, [this]() { emit windowMaximizedChanged(); });
    }
}

QString SettingsViewModel::themeMode() const { return m_dependencies.themeController ? m_dependencies.themeController->mode() : QStringLiteral("system"); }
QString SettingsViewModel::effectiveTheme() const { return m_dependencies.themeController ? m_dependencies.themeController->effectiveTheme() : QStringLiteral("light"); }
QString SettingsViewModel::languageMode() const { return m_dependencies.languageController ? m_dependencies.languageController->mode() : QStringLiteral("system"); }
int SettingsViewModel::messageRetentionLimit() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->messageRetentionLimit() : 5000; }
int SettingsViewModel::logRetentionLimit() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->logRetentionLimit() : 2000; }
int SettingsViewModel::historyPageSize() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->historyPageSize() : 500; }
int SettingsViewModel::maxIncomingPayloadBytes() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->maxIncomingPayloadBytes() : 1024 * 1024; }
bool SettingsViewModel::deleteHistoryWithSession() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->deleteHistoryWithSession() : true; }
bool SettingsViewModel::saveMessagesWhenOutputPaused() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->saveMessagesWhenOutputPaused() : true; }
QString SettingsViewModel::clearMessagesOnExit() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->clearMessagesOnExit() : QStringLiteral("never"); }
QString SettingsViewModel::clearLogsOnExit() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->clearLogsOnExit() : QStringLiteral("never"); }
int SettingsViewModel::windowWidth() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->windowWidth() : 1480; }
int SettingsViewModel::windowHeight() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->windowHeight() : 820; }
bool SettingsViewModel::windowMaximized() const { return m_dependencies.preferencesController && m_dependencies.preferencesController->windowMaximized(); }
int SettingsViewModel::themeModeIndex() const { return optionIndex(themeModeValues(), themeMode()); }
int SettingsViewModel::languageModeIndex() const { return optionIndex(languageModeValues(), languageMode()); }
int SettingsViewModel::messageRetentionLimitIndex() const { return optionIndex(messageRetentionLimitValues(), messageRetentionLimit()); }
int SettingsViewModel::logRetentionLimitIndex() const { return optionIndex(logRetentionLimitValues(), logRetentionLimit()); }
int SettingsViewModel::historyPageSizeIndex() const { return optionIndex(historyPageSizeValues(), historyPageSize()); }
int SettingsViewModel::maxIncomingPayloadBytesIndex() const { return optionIndex(maxIncomingPayloadByteValues(), maxIncomingPayloadBytes()); }
int SettingsViewModel::clearMessagesOnExitIndex() const { return optionIndex(cleanupModeValues(), clearMessagesOnExit()); }
int SettingsViewModel::clearLogsOnExitIndex() const { return optionIndex(cleanupModeValues(), clearLogsOnExit()); }

void SettingsViewModel::setThemeMode(const QString &mode)
{
    if (m_dependencies.themeController) {
        m_dependencies.themeController->setMode(mode);
    }
}

void SettingsViewModel::setLanguageMode(const QString &mode)
{
    if (m_dependencies.languageController) {
        m_dependencies.languageController->setMode(mode);
    }
}

void SettingsViewModel::setMessageRetentionLimit(int limit)
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

void SettingsViewModel::setLogRetentionLimit(int limit)
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

void SettingsViewModel::setHistoryPageSize(int pageSize)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setHistoryPageSize(pageSize);
    }
}

void SettingsViewModel::setMaxIncomingPayloadBytes(int bytes)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setMaxIncomingPayloadBytes(bytes);
    }
}

void SettingsViewModel::setDeleteHistoryWithSession(bool enabled)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setDeleteHistoryWithSession(enabled);
    }
}

void SettingsViewModel::setSaveMessagesWhenOutputPaused(bool enabled)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setSaveMessagesWhenOutputPaused(enabled);
    }
}

void SettingsViewModel::setClearMessagesOnExit(const QString &mode)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setClearMessagesOnExit(mode);
    }
}

void SettingsViewModel::setClearLogsOnExit(const QString &mode)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setClearLogsOnExit(mode);
    }
}

void SettingsViewModel::setWindowMaximized(bool maximized)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setWindowMaximized(maximized);
    }
}

void SettingsViewModel::saveWindowGeometry(int width, int height)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setWindowGeometry(width, height);
    }
}
void SettingsViewModel::setThemeModeIndex(int index) { setThemeMode(optionValue(themeModeValues(), index).toString()); }
void SettingsViewModel::setLanguageModeIndex(int index) { setLanguageMode(optionValue(languageModeValues(), index).toString()); }
void SettingsViewModel::setMessageRetentionLimitIndex(int index) { setMessageRetentionLimit(optionValue(messageRetentionLimitValues(), index).toInt()); }
void SettingsViewModel::setLogRetentionLimitIndex(int index) { setLogRetentionLimit(optionValue(logRetentionLimitValues(), index).toInt()); }
void SettingsViewModel::setHistoryPageSizeIndex(int index) { setHistoryPageSize(optionValue(historyPageSizeValues(), index).toInt()); }
void SettingsViewModel::setMaxIncomingPayloadBytesIndex(int index) { setMaxIncomingPayloadBytes(optionValue(maxIncomingPayloadByteValues(), index).toInt()); }
void SettingsViewModel::setClearMessagesOnExitIndex(int index) { setClearMessagesOnExit(optionValue(cleanupModeValues(), index).toString()); }
void SettingsViewModel::setClearLogsOnExitIndex(int index) { setClearLogsOnExit(optionValue(cleanupModeValues(), index).toString()); }
void SettingsViewModel::clearAllMessages()
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

void SettingsViewModel::clearAllLogs()
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

void SettingsViewModel::clearAllHistory()
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
