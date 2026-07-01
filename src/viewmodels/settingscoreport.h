#pragma once

#include <functional>

#include <QString>

class QObject;

struct SettingsCoreSignalHandlers
{
    std::function<void()> themeModeChanged;
    std::function<void()> effectiveThemeChanged;
    std::function<void()> languageModeChanged;
    std::function<void()> languageChanged;
    std::function<void()> messageRetentionLimitChanged;
    std::function<void()> logRetentionLimitChanged;
    std::function<void()> historyPageSizeChanged;
    std::function<void()> maxIncomingPayloadBytesChanged;
    std::function<void()> deleteHistoryWithSessionChanged;
    std::function<void()> saveMessagesWhenOutputPausedChanged;
    std::function<void()> clearMessagesOnExitChanged;
    std::function<void()> clearLogsOnExitChanged;
    std::function<void()> windowWidthChanged;
    std::function<void()> windowHeightChanged;
    std::function<void()> windowMaximizedChanged;
};

class SettingsCorePort
{
public:
    virtual ~SettingsCorePort() = default;

    virtual void bindSettingsSignals(QObject *context, const SettingsCoreSignalHandlers &handlers) = 0;

    virtual QString themeMode() const = 0;
    virtual QString effectiveTheme() const = 0;
    virtual QString languageMode() const = 0;
    virtual int messageRetentionLimit() const = 0;
    virtual int logRetentionLimit() const = 0;
    virtual int historyPageSize() const = 0;
    virtual int maxIncomingPayloadBytes() const = 0;
    virtual bool deleteHistoryWithSession() const = 0;
    virtual bool saveMessagesWhenOutputPaused() const = 0;
    virtual QString clearMessagesOnExit() const = 0;
    virtual QString clearLogsOnExit() const = 0;
    virtual int windowWidth() const = 0;
    virtual int windowHeight() const = 0;
    virtual bool windowMaximized() const = 0;

    virtual void setThemeMode(const QString &mode) = 0;
    virtual void setLanguageMode(const QString &mode) = 0;
    virtual void setMessageRetentionLimit(int limit) = 0;
    virtual void setLogRetentionLimit(int limit) = 0;
    virtual void setHistoryPageSize(int pageSize) = 0;
    virtual void setMaxIncomingPayloadBytes(int bytes) = 0;
    virtual void setDeleteHistoryWithSession(bool enabled) = 0;
    virtual void setSaveMessagesWhenOutputPaused(bool enabled) = 0;
    virtual void setClearMessagesOnExit(const QString &mode) = 0;
    virtual void setClearLogsOnExit(const QString &mode) = 0;
    virtual void setWindowMaximized(bool maximized) = 0;
    virtual void saveWindowGeometry(int width, int height) = 0;
    virtual void clearAllMessages() = 0;
    virtual void clearAllLogs() = 0;
    virtual void clearAllHistory() = 0;
};
