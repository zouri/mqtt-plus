#include "app/applicationnotifier.h"

ApplicationNotifier::ApplicationNotifier(QObject *parent)
    : QObject(parent)
{
}

void ApplicationNotifier::notifySessionsChanged() { emit sessionsChanged(); }
void ApplicationNotifier::notifyCurrentSessionIndexChanged() { emit currentSessionIndexChanged(); }
void ApplicationNotifier::notifyCurrentSessionChanged() { emit currentSessionChanged(); }
void ApplicationNotifier::notifySubscriptionsChanged() { emit subscriptionsChanged(); }
void ApplicationNotifier::notifyMessageStreamChanged() { emit messageStreamChanged(); }
void ApplicationNotifier::notifyLogStreamChanged() { emit logStreamChanged(); }
void ApplicationNotifier::notifyMessageStreamRowAppended(const QVariantMap &row) { emit messageStreamRowAppended(row); }
void ApplicationNotifier::notifyLogStreamRowAppended(const QVariantMap &row) { emit logStreamRowAppended(row); }
void ApplicationNotifier::notifyScriptLibraryChanged() { emit scriptLibraryChanged(); }
void ApplicationNotifier::notifyThemeModeChanged() { emit themeModeChanged(); }
void ApplicationNotifier::notifyEffectiveThemeChanged() { emit effectiveThemeChanged(); }
void ApplicationNotifier::notifyLanguageModeChanged() { emit languageModeChanged(); }
void ApplicationNotifier::notifyLanguageChanged() { emit languageChanged(); }
void ApplicationNotifier::notifyMessageRetentionLimitChanged() { emit messageRetentionLimitChanged(); }
void ApplicationNotifier::notifyLogRetentionLimitChanged() { emit logRetentionLimitChanged(); }
void ApplicationNotifier::notifyHistoryPageSizeChanged() { emit historyPageSizeChanged(); }
void ApplicationNotifier::notifyMaxIncomingPayloadBytesChanged() { emit maxIncomingPayloadBytesChanged(); }
void ApplicationNotifier::notifyDeleteHistoryWithSessionChanged() { emit deleteHistoryWithSessionChanged(); }
void ApplicationNotifier::notifySaveMessagesWhenOutputPausedChanged() { emit saveMessagesWhenOutputPausedChanged(); }
void ApplicationNotifier::notifyClearMessagesOnExitChanged() { emit clearMessagesOnExitChanged(); }
void ApplicationNotifier::notifyClearLogsOnExitChanged() { emit clearLogsOnExitChanged(); }
void ApplicationNotifier::notifyWindowWidthChanged() { emit windowWidthChanged(); }
void ApplicationNotifier::notifyWindowHeightChanged() { emit windowHeightChanged(); }
void ApplicationNotifier::notifyWindowMaximizedChanged() { emit windowMaximizedChanged(); }
