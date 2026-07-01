#pragma once

#include "domain/session.h"
#include "viewmodels/settingscoreport.h"

#include <QObject>
#include <QVector>

#include <functional>

class EventController;
class EventStreamModel;
class HistoryStore;
class LanguageController;
class PreferencesController;
class ThemeController;

struct SettingsWorkspaceDependencies {
    ThemeController *themeController = nullptr;
    LanguageController *languageController = nullptr;
    PreferencesController *preferencesController = nullptr;
    EventController *eventController = nullptr;
    HistoryStore *historyStore = nullptr;
    QVector<SessionState> *sessions = nullptr;
    EventStreamModel *messages = nullptr;
    EventStreamModel *logs = nullptr;

    std::function<void(QObject *, std::function<void()>)> bindThemeModeChanged;
    std::function<void(QObject *, std::function<void()>)> bindEffectiveThemeChanged;
    std::function<void(QObject *, std::function<void()>)> bindLanguageModeChanged;
    std::function<void(QObject *, std::function<void()>)> bindLanguageChanged;
    std::function<void(QObject *, std::function<void()>)> bindMessageRetentionLimitChanged;
    std::function<void(QObject *, std::function<void()>)> bindLogRetentionLimitChanged;
    std::function<void(QObject *, std::function<void()>)> bindHistoryPageSizeChanged;
    std::function<void(QObject *, std::function<void()>)> bindMaxIncomingPayloadBytesChanged;
    std::function<void(QObject *, std::function<void()>)> bindDeleteHistoryWithSessionChanged;
    std::function<void(QObject *, std::function<void()>)> bindSaveMessagesWhenOutputPausedChanged;
    std::function<void(QObject *, std::function<void()>)> bindClearMessagesOnExitChanged;
    std::function<void(QObject *, std::function<void()>)> bindClearLogsOnExitChanged;
    std::function<void(QObject *, std::function<void()>)> bindWindowWidthChanged;
    std::function<void(QObject *, std::function<void()>)> bindWindowHeightChanged;
    std::function<void(QObject *, std::function<void()>)> bindWindowMaximizedChanged;

    std::function<void()> reloadCurrentSessionHistory;
    std::function<void()> refreshScriptTestSamplesModel;
    std::function<void()> emitMessageStreamChanged;
    std::function<void()> emitLogStreamChanged;
};

class SettingsWorkspace : public SettingsCorePort
{
public:
    explicit SettingsWorkspace(const SettingsWorkspaceDependencies &dependencies = {});

    void bindSettingsSignals(QObject *context, const SettingsCoreSignalHandlers &handlers) override;

    QString themeMode() const override;
    QString effectiveTheme() const override;
    QString languageMode() const override;
    int messageRetentionLimit() const override;
    int logRetentionLimit() const override;
    int historyPageSize() const override;
    int maxIncomingPayloadBytes() const override;
    bool deleteHistoryWithSession() const override;
    bool saveMessagesWhenOutputPaused() const override;
    QString clearMessagesOnExit() const override;
    QString clearLogsOnExit() const override;
    int windowWidth() const override;
    int windowHeight() const override;
    bool windowMaximized() const override;

    void setThemeMode(const QString &mode) override;
    void setLanguageMode(const QString &mode) override;
    void setMessageRetentionLimit(int limit) override;
    void setLogRetentionLimit(int limit) override;
    void setHistoryPageSize(int pageSize) override;
    void setMaxIncomingPayloadBytes(int bytes) override;
    void setDeleteHistoryWithSession(bool enabled) override;
    void setSaveMessagesWhenOutputPaused(bool enabled) override;
    void setClearMessagesOnExit(const QString &mode) override;
    void setClearLogsOnExit(const QString &mode) override;
    void setWindowMaximized(bool maximized) override;
    void saveWindowGeometry(int width, int height) override;
    void clearAllMessages() override;
    void clearAllLogs() override;
    void clearAllHistory() override;

private:
    SettingsWorkspaceDependencies m_dependencies;
};
