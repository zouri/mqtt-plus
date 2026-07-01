#include "app/applicationcore.h"

#include "app/applicationcorestate.h"
#include "services/apputils.h"

#include <memory>

using namespace AppUtils;

ApplicationCore::ApplicationCore(QObject *parent)
    : QObject(parent)
    , m_state(std::make_unique<ApplicationCoreState>(*this))
{
    m_state->launchTimestamp = timestampNow();
    m_state->installSignalBindings();
    m_state->runStartup();
}

ApplicationCore::~ApplicationCore()
{
    m_state->applyExitCleanup();
}

void ApplicationCore::notifySessionsChanged() { emit sessionsChanged(); }
void ApplicationCore::notifyCurrentSessionIndexChanged() { emit currentSessionIndexChanged(); }
void ApplicationCore::notifyCurrentSessionChanged() { emit currentSessionChanged(); }
void ApplicationCore::notifySubscriptionsChanged() { emit subscriptionsChanged(); }
void ApplicationCore::notifyMessageStreamChanged() { emit messageStreamChanged(); }
void ApplicationCore::notifyLogStreamChanged() { emit logStreamChanged(); }
void ApplicationCore::notifyMessageStreamRowAppended(const QVariantMap &row) { emit messageStreamRowAppended(row); }
void ApplicationCore::notifyLogStreamRowAppended(const QVariantMap &row) { emit logStreamRowAppended(row); }
void ApplicationCore::notifyScriptLibraryChanged() { emit scriptLibraryChanged(); }
void ApplicationCore::notifyThemeModeChanged() { emit themeModeChanged(); }
void ApplicationCore::notifyEffectiveThemeChanged() { emit effectiveThemeChanged(); }
void ApplicationCore::notifyLanguageModeChanged() { emit languageModeChanged(); }
void ApplicationCore::notifyLanguageChanged() { emit languageChanged(); }
void ApplicationCore::notifyMessageRetentionLimitChanged() { emit messageRetentionLimitChanged(); }
void ApplicationCore::notifyLogRetentionLimitChanged() { emit logRetentionLimitChanged(); }
void ApplicationCore::notifyHistoryPageSizeChanged() { emit historyPageSizeChanged(); }
void ApplicationCore::notifyMaxIncomingPayloadBytesChanged() { emit maxIncomingPayloadBytesChanged(); }
void ApplicationCore::notifyDeleteHistoryWithSessionChanged() { emit deleteHistoryWithSessionChanged(); }
void ApplicationCore::notifySaveMessagesWhenOutputPausedChanged() { emit saveMessagesWhenOutputPausedChanged(); }
void ApplicationCore::notifyClearMessagesOnExitChanged() { emit clearMessagesOnExitChanged(); }
void ApplicationCore::notifyClearLogsOnExitChanged() { emit clearLogsOnExitChanged(); }
void ApplicationCore::notifyWindowWidthChanged() { emit windowWidthChanged(); }
void ApplicationCore::notifyWindowHeightChanged() { emit windowHeightChanged(); }
void ApplicationCore::notifyWindowMaximizedChanged() { emit windowMaximizedChanged(); }
