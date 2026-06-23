#include "app/appeventbus.h"

#include "app/appfacade.h"

AppEventBus::AppEventBus(AppFacade *facade, QObject *parent)
    : QObject(parent)
    , m_facade(facade)
{
    connect(this, &AppEventBus::themeModeChanged, this, &AppEventBus::settingsChanged);
    connect(this, &AppEventBus::effectiveThemeChanged, this, &AppEventBus::settingsChanged);
    connect(this, &AppEventBus::languageModeChanged, this, &AppEventBus::settingsChanged);
    connect(this, &AppEventBus::messageRetentionLimitChanged, this, &AppEventBus::settingsChanged);
    connect(this, &AppEventBus::logRetentionLimitChanged, this, &AppEventBus::settingsChanged);
    connect(this, &AppEventBus::historyPageSizeChanged, this, &AppEventBus::settingsChanged);
    connect(this, &AppEventBus::deleteHistoryWithSessionChanged, this, &AppEventBus::settingsChanged);
    connect(this, &AppEventBus::saveMessagesWhenOutputPausedChanged, this, &AppEventBus::settingsChanged);
    connect(this, &AppEventBus::clearMessagesOnExitChanged, this, &AppEventBus::settingsChanged);
    connect(this, &AppEventBus::clearLogsOnExitChanged, this, &AppEventBus::settingsChanged);
    connect(this, &AppEventBus::windowWidthChanged, this, &AppEventBus::settingsChanged);
    connect(this, &AppEventBus::windowHeightChanged, this, &AppEventBus::settingsChanged);
    connect(this, &AppEventBus::windowMaximizedChanged, this, &AppEventBus::settingsChanged);
}

WorkbenchBus *AppEventBus::workbench()
{
    return m_facade->workbench();
}

SettingsBus *AppEventBus::settings()
{
    return m_facade->settings();
}

ScriptsBus *AppEventBus::scripts()
{
    return m_facade->scripts();
}

LogsBus *AppEventBus::logs()
{
    return m_facade->logs();
}

void AppEventBus::publishSessionsChanged()
{
    emit sessionsChanged();
}

void AppEventBus::publishCurrentSessionIndexChanged()
{
    emit currentSessionIndexChanged();
}

void AppEventBus::publishCurrentSessionChanged()
{
    emit currentSessionChanged();
}

void AppEventBus::publishSubscriptionsChanged()
{
    emit subscriptionsChanged();
}

void AppEventBus::publishMessageStreamChanged()
{
    emit messageStreamChanged();
}

void AppEventBus::publishLogsChanged()
{
    emit logsChanged();
}

void AppEventBus::publishMessageStreamRowAppended(const QVariantMap &row)
{
    emit messageStreamRowAppended(row);
}

void AppEventBus::publishLogsRowAppended(const QVariantMap &row)
{
    emit logsRowAppended(row);
}

void AppEventBus::publishScriptsChanged()
{
    emit scriptsChanged();
}

void AppEventBus::publishScriptTestSamplesChanged()
{
    emit scriptTestSamplesChanged();
}

void AppEventBus::publishThemeModeChanged()
{
    emit themeModeChanged();
}

void AppEventBus::publishEffectiveThemeChanged()
{
    emit effectiveThemeChanged();
}

void AppEventBus::publishLanguageModeChanged()
{
    emit languageModeChanged();
}

void AppEventBus::publishSettingsChanged()
{
    emit settingsChanged();
}

void AppEventBus::publishLanguageChanged()
{
    emit languageChanged();
}

void AppEventBus::publishMessageRetentionLimitChanged()
{
    emit messageRetentionLimitChanged();
}

void AppEventBus::publishLogRetentionLimitChanged()
{
    emit logRetentionLimitChanged();
}

void AppEventBus::publishHistoryPageSizeChanged()
{
    emit historyPageSizeChanged();
}

void AppEventBus::publishDeleteHistoryWithSessionChanged()
{
    emit deleteHistoryWithSessionChanged();
}

void AppEventBus::publishSaveMessagesWhenOutputPausedChanged()
{
    emit saveMessagesWhenOutputPausedChanged();
}

void AppEventBus::publishClearMessagesOnExitChanged()
{
    emit clearMessagesOnExitChanged();
}

void AppEventBus::publishClearLogsOnExitChanged()
{
    emit clearLogsOnExitChanged();
}

void AppEventBus::publishWindowWidthChanged()
{
    emit windowWidthChanged();
}

void AppEventBus::publishWindowHeightChanged()
{
    emit windowHeightChanged();
}

void AppEventBus::publishWindowMaximizedChanged()
{
    emit windowMaximizedChanged();
}
