#include "app/uieventhub.h"

UiEventHub::UiEventHub(QObject *parent)
    : QObject(parent)
{
    connect(this, &UiEventHub::themeModeChanged, this, &UiEventHub::settingsChanged);
    connect(this, &UiEventHub::effectiveThemeChanged, this, &UiEventHub::settingsChanged);
    connect(this, &UiEventHub::languageModeChanged, this, &UiEventHub::settingsChanged);
    connect(this, &UiEventHub::messageRetentionLimitChanged, this, &UiEventHub::settingsChanged);
    connect(this, &UiEventHub::logRetentionLimitChanged, this, &UiEventHub::settingsChanged);
    connect(this, &UiEventHub::historyPageSizeChanged, this, &UiEventHub::settingsChanged);
    connect(this, &UiEventHub::deleteHistoryWithSessionChanged, this, &UiEventHub::settingsChanged);
    connect(this, &UiEventHub::saveMessagesWhenOutputPausedChanged, this, &UiEventHub::settingsChanged);
    connect(this, &UiEventHub::clearMessagesOnExitChanged, this, &UiEventHub::settingsChanged);
    connect(this, &UiEventHub::clearLogsOnExitChanged, this, &UiEventHub::settingsChanged);
    connect(this, &UiEventHub::windowWidthChanged, this, &UiEventHub::settingsChanged);
    connect(this, &UiEventHub::windowHeightChanged, this, &UiEventHub::settingsChanged);
    connect(this, &UiEventHub::windowMaximizedChanged, this, &UiEventHub::settingsChanged);
}

void UiEventHub::publishSessionsChanged()
{
    emit sessionsChanged();
}

void UiEventHub::publishCurrentSessionIndexChanged()
{
    emit currentSessionIndexChanged();
}

void UiEventHub::publishCurrentSessionChanged()
{
    emit currentSessionChanged();
}

void UiEventHub::publishSubscriptionsChanged()
{
    emit subscriptionsChanged();
}

void UiEventHub::publishMessageStreamChanged()
{
    emit messageStreamChanged();
}

void UiEventHub::publishLogsChanged()
{
    emit logsChanged();
}

void UiEventHub::publishMessageStreamRowAppended(const QVariantMap &row)
{
    emit messageStreamRowAppended(row);
}

void UiEventHub::publishLogsRowAppended(const QVariantMap &row)
{
    emit logsRowAppended(row);
}

void UiEventHub::publishScriptsChanged()
{
    emit scriptsChanged();
}

void UiEventHub::publishScriptTestSamplesChanged()
{
    emit scriptTestSamplesChanged();
}

void UiEventHub::publishThemeModeChanged()
{
    emit themeModeChanged();
}

void UiEventHub::publishEffectiveThemeChanged()
{
    emit effectiveThemeChanged();
}

void UiEventHub::publishLanguageModeChanged()
{
    emit languageModeChanged();
}

void UiEventHub::publishSettingsChanged()
{
    emit settingsChanged();
}

void UiEventHub::publishLanguageChanged()
{
    emit languageChanged();
}

void UiEventHub::publishMessageRetentionLimitChanged()
{
    emit messageRetentionLimitChanged();
}

void UiEventHub::publishLogRetentionLimitChanged()
{
    emit logRetentionLimitChanged();
}

void UiEventHub::publishHistoryPageSizeChanged()
{
    emit historyPageSizeChanged();
}

void UiEventHub::publishDeleteHistoryWithSessionChanged()
{
    emit deleteHistoryWithSessionChanged();
}

void UiEventHub::publishSaveMessagesWhenOutputPausedChanged()
{
    emit saveMessagesWhenOutputPausedChanged();
}

void UiEventHub::publishClearMessagesOnExitChanged()
{
    emit clearMessagesOnExitChanged();
}

void UiEventHub::publishClearLogsOnExitChanged()
{
    emit clearLogsOnExitChanged();
}

void UiEventHub::publishWindowWidthChanged()
{
    emit windowWidthChanged();
}

void UiEventHub::publishWindowHeightChanged()
{
    emit windowHeightChanged();
}

void UiEventHub::publishWindowMaximizedChanged()
{
    emit windowMaximizedChanged();
}
