#include "viewmodels/settingsviewmodel.h"

#include "viewmodels/settingscoreport.h"

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

SettingsViewModel::SettingsViewModel(SettingsCorePort *core, QObject *parent)
    : SettingsOptionsViewModel(parent)
    , m_core(core)
{
    if (m_core) {
        m_core->bindSettingsSignals(this, {
            [this]() { emit themeModeChanged(); },
            [this]() { emit effectiveThemeChanged(); },
            [this]() { emit languageModeChanged(); },
            [this]() { emit languageChanged(); },
            [this]() { emit messageRetentionLimitChanged(); },
            [this]() { emit logRetentionLimitChanged(); },
            [this]() { emit historyPageSizeChanged(); },
            [this]() { emit maxIncomingPayloadBytesChanged(); },
            [this]() { emit deleteHistoryWithSessionChanged(); },
            [this]() { emit saveMessagesWhenOutputPausedChanged(); },
            [this]() { emit clearMessagesOnExitChanged(); },
            [this]() { emit clearLogsOnExitChanged(); },
            [this]() { emit windowWidthChanged(); },
            [this]() { emit windowHeightChanged(); },
            [this]() { emit windowMaximizedChanged(); },
        });
    }
}

QString SettingsViewModel::themeMode() const { return m_core ? m_core->themeMode() : QStringLiteral("system"); }
QString SettingsViewModel::effectiveTheme() const { return m_core ? m_core->effectiveTheme() : QStringLiteral("light"); }
QString SettingsViewModel::languageMode() const { return m_core ? m_core->languageMode() : QStringLiteral("system"); }
int SettingsViewModel::messageRetentionLimit() const { return m_core ? m_core->messageRetentionLimit() : 5000; }
int SettingsViewModel::logRetentionLimit() const { return m_core ? m_core->logRetentionLimit() : 2000; }
int SettingsViewModel::historyPageSize() const { return m_core ? m_core->historyPageSize() : 500; }
int SettingsViewModel::maxIncomingPayloadBytes() const { return m_core ? m_core->maxIncomingPayloadBytes() : 1024 * 1024; }
bool SettingsViewModel::deleteHistoryWithSession() const { return m_core ? m_core->deleteHistoryWithSession() : true; }
bool SettingsViewModel::saveMessagesWhenOutputPaused() const { return m_core ? m_core->saveMessagesWhenOutputPaused() : true; }
QString SettingsViewModel::clearMessagesOnExit() const { return m_core ? m_core->clearMessagesOnExit() : QStringLiteral("never"); }
QString SettingsViewModel::clearLogsOnExit() const { return m_core ? m_core->clearLogsOnExit() : QStringLiteral("never"); }
int SettingsViewModel::windowWidth() const { return m_core ? m_core->windowWidth() : 1480; }
int SettingsViewModel::windowHeight() const { return m_core ? m_core->windowHeight() : 820; }
bool SettingsViewModel::windowMaximized() const { return m_core && m_core->windowMaximized(); }
int SettingsViewModel::themeModeIndex() const { return optionIndex(themeModeValues(), themeMode()); }
int SettingsViewModel::languageModeIndex() const { return optionIndex(languageModeValues(), languageMode()); }
int SettingsViewModel::messageRetentionLimitIndex() const { return optionIndex(messageRetentionLimitValues(), messageRetentionLimit()); }
int SettingsViewModel::logRetentionLimitIndex() const { return optionIndex(logRetentionLimitValues(), logRetentionLimit()); }
int SettingsViewModel::historyPageSizeIndex() const { return optionIndex(historyPageSizeValues(), historyPageSize()); }
int SettingsViewModel::maxIncomingPayloadBytesIndex() const { return optionIndex(maxIncomingPayloadByteValues(), maxIncomingPayloadBytes()); }
int SettingsViewModel::clearMessagesOnExitIndex() const { return optionIndex(cleanupModeValues(), clearMessagesOnExit()); }
int SettingsViewModel::clearLogsOnExitIndex() const { return optionIndex(cleanupModeValues(), clearLogsOnExit()); }

void SettingsViewModel::setThemeMode(const QString &mode) { if (m_core) { m_core->setThemeMode(mode); } }
void SettingsViewModel::setLanguageMode(const QString &mode) { if (m_core) { m_core->setLanguageMode(mode); } }
void SettingsViewModel::setMessageRetentionLimit(int limit) { if (m_core) { m_core->setMessageRetentionLimit(limit); } }
void SettingsViewModel::setLogRetentionLimit(int limit) { if (m_core) { m_core->setLogRetentionLimit(limit); } }
void SettingsViewModel::setHistoryPageSize(int pageSize) { if (m_core) { m_core->setHistoryPageSize(pageSize); } }
void SettingsViewModel::setMaxIncomingPayloadBytes(int bytes) { if (m_core) { m_core->setMaxIncomingPayloadBytes(bytes); } }
void SettingsViewModel::setDeleteHistoryWithSession(bool enabled) { if (m_core) { m_core->setDeleteHistoryWithSession(enabled); } }
void SettingsViewModel::setSaveMessagesWhenOutputPaused(bool enabled) { if (m_core) { m_core->setSaveMessagesWhenOutputPaused(enabled); } }
void SettingsViewModel::setClearMessagesOnExit(const QString &mode) { if (m_core) { m_core->setClearMessagesOnExit(mode); } }
void SettingsViewModel::setClearLogsOnExit(const QString &mode) { if (m_core) { m_core->setClearLogsOnExit(mode); } }
void SettingsViewModel::setWindowMaximized(bool maximized) { if (m_core) { m_core->setWindowMaximized(maximized); } }
void SettingsViewModel::saveWindowGeometry(int width, int height) { if (m_core) { m_core->saveWindowGeometry(width, height); } }
void SettingsViewModel::setThemeModeIndex(int index) { setThemeMode(optionValue(themeModeValues(), index).toString()); }
void SettingsViewModel::setLanguageModeIndex(int index) { setLanguageMode(optionValue(languageModeValues(), index).toString()); }
void SettingsViewModel::setMessageRetentionLimitIndex(int index) { setMessageRetentionLimit(optionValue(messageRetentionLimitValues(), index).toInt()); }
void SettingsViewModel::setLogRetentionLimitIndex(int index) { setLogRetentionLimit(optionValue(logRetentionLimitValues(), index).toInt()); }
void SettingsViewModel::setHistoryPageSizeIndex(int index) { setHistoryPageSize(optionValue(historyPageSizeValues(), index).toInt()); }
void SettingsViewModel::setMaxIncomingPayloadBytesIndex(int index) { setMaxIncomingPayloadBytes(optionValue(maxIncomingPayloadByteValues(), index).toInt()); }
void SettingsViewModel::setClearMessagesOnExitIndex(int index) { setClearMessagesOnExit(optionValue(cleanupModeValues(), index).toString()); }
void SettingsViewModel::setClearLogsOnExitIndex(int index) { setClearLogsOnExit(optionValue(cleanupModeValues(), index).toString()); }
void SettingsViewModel::clearAllMessages() { if (m_core) { m_core->clearAllMessages(); } }
void SettingsViewModel::clearAllLogs() { if (m_core) { m_core->clearAllLogs(); } }
void SettingsViewModel::clearAllHistory() { if (m_core) { m_core->clearAllHistory(); } }
