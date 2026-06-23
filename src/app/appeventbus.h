#pragma once

#include <QObject>
#include <QVariantMap>

#include "app/settingsbus.h"
#include "app/logsbus.h"
#include "app/scriptsbus.h"
#include "app/workbenchbus.h"

class AppFacade;

class AppEventBus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WorkbenchBus* workbench READ workbench CONSTANT)
    Q_PROPERTY(SettingsBus* settings READ settings CONSTANT)
    Q_PROPERTY(ScriptsBus* scripts READ scripts CONSTANT)
    Q_PROPERTY(LogsBus* logs READ logs CONSTANT)

public:
    explicit AppEventBus(AppFacade *facade, QObject *parent = nullptr);

    WorkbenchBus *workbench();
    SettingsBus *settings();
    ScriptsBus *scripts();
    LogsBus *logs();

    void publishSessionsChanged();
    void publishCurrentSessionIndexChanged();
    void publishCurrentSessionChanged();
    void publishSubscriptionsChanged();
    void publishMessageStreamChanged();
    void publishLogsChanged();
    void publishMessageStreamRowAppended(const QVariantMap &row);
    void publishLogsRowAppended(const QVariantMap &row);
    void publishScriptsChanged();
    void publishScriptTestSamplesChanged();
    void publishThemeModeChanged();
    void publishEffectiveThemeChanged();
    void publishLanguageModeChanged();
    void publishSettingsChanged();
    void publishLanguageChanged();
    void publishMessageRetentionLimitChanged();
    void publishLogRetentionLimitChanged();
    void publishHistoryPageSizeChanged();
    void publishDeleteHistoryWithSessionChanged();
    void publishSaveMessagesWhenOutputPausedChanged();
    void publishClearMessagesOnExitChanged();
    void publishClearLogsOnExitChanged();
    void publishWindowWidthChanged();
    void publishWindowHeightChanged();
    void publishWindowMaximizedChanged();

signals:
    void sessionsChanged();
    void currentSessionIndexChanged();
    void currentSessionChanged();
    void subscriptionsChanged();
    void messageStreamChanged();
    void logsChanged();
    void messageStreamRowAppended(const QVariantMap &row);
    void logsRowAppended(const QVariantMap &row);
    void scriptsChanged();
    void scriptTestSamplesChanged();
    void settingsChanged();
    void themeModeChanged();
    void effectiveThemeChanged();
    void languageModeChanged();
    void languageChanged();
    void messageRetentionLimitChanged();
    void logRetentionLimitChanged();
    void historyPageSizeChanged();
    void deleteHistoryWithSessionChanged();
    void saveMessagesWhenOutputPausedChanged();
    void clearMessagesOnExitChanged();
    void clearLogsOnExitChanged();
    void windowWidthChanged();
    void windowHeightChanged();
    void windowMaximizedChanged();

private:
    AppFacade *m_facade = nullptr;
};
