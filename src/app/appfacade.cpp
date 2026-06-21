#include "app/appfacade.h"

#include "app/appfacadeutils.h"

#include <QClipboard>
#include <QGuiApplication>

using namespace AppFacadeUtils;

AppFacade::AppFacade(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"))
    , m_sessionController(this)
    , m_scriptController(this)
    , m_subscriptionController(this, this)
    , m_mqttController(this, this)
    , m_eventController(this, this)
    , m_themeController(&m_settings, this)
    , m_languageController(&m_settings, this)
    , m_preferencesController(&m_settings, this)
    , m_sessionsModel(this)
    , m_subscriptionsModel(this)
    , m_filteredSubscriptionsModel(this)
    , m_messagesModel(this)
    , m_logsModel(this)
    , m_scriptsModel(this)
    , m_scriptTestSamplesModel(this)
{
    m_sessionController.setFacade(this);
    m_filteredSubscriptionsModel.setSourceModel(&m_subscriptionsModel);
    m_launchTimestamp = timestampNow();
    connect(&m_scriptController, &ScriptController::storageError, this, &AppFacade::reportStorageError);
    connect(&m_themeController, &ThemeController::modeChanged, this, &AppFacade::themeModeChanged);
    connect(&m_themeController, &ThemeController::effectiveThemeChanged, this, &AppFacade::effectiveThemeChanged);
    connect(&m_languageController, &LanguageController::modeChanged, this, &AppFacade::languageModeChanged);
    connect(&m_languageController, &LanguageController::languageChanged, this, [this]() {
        refreshSessionsModel();
        refreshSubscriptionsModel();
        emit currentSessionChanged();
        emit sessionsChanged();
        emit subscriptionsChanged();
        emit languageChanged();
    });
    connect(&m_preferencesController, &PreferencesController::messageRetentionLimitChanged, this, &AppFacade::messageRetentionLimitChanged);
    connect(&m_preferencesController, &PreferencesController::logRetentionLimitChanged, this, &AppFacade::logRetentionLimitChanged);
    connect(&m_preferencesController, &PreferencesController::historyPageSizeChanged, this, [this]() {
        reloadCurrentSessionHistory();
        emit messageStreamChanged();
        emit logStreamChanged();
        emit historyPageSizeChanged();
    });
    connect(&m_preferencesController, &PreferencesController::maxIncomingPayloadBytesChanged, this, &AppFacade::maxIncomingPayloadBytesChanged);
    connect(&m_preferencesController, &PreferencesController::deleteHistoryWithSessionChanged, this, &AppFacade::deleteHistoryWithSessionChanged);
    connect(&m_preferencesController, &PreferencesController::saveMessagesWhenOutputPausedChanged, this, &AppFacade::saveMessagesWhenOutputPausedChanged);
    connect(&m_preferencesController, &PreferencesController::clearMessagesOnExitChanged, this, &AppFacade::clearMessagesOnExitChanged);
    connect(&m_preferencesController, &PreferencesController::clearLogsOnExitChanged, this, &AppFacade::clearLogsOnExitChanged);

    m_subscriptionFpsRefreshTimer.setInterval(kSubscriptionFpsRefreshIntervalMs);
    connect(
        &m_subscriptionFpsRefreshTimer,
        &QTimer::timeout,
        this,
        &AppFacade::refreshSubscriptionFps);
    loadScripts();
    loadSessions();
}

AppFacade::~AppFacade()
{
    applyExitCleanup();
}

void AppFacade::applyExitCleanup()
{
    m_eventController.flushPendingMessageHistory();

    const auto clearMessages = [this](const QString &mode) {
        if (mode == QStringLiteral("all")) {
            m_historyStore.clearAllMessages();
        } else if (mode == QStringLiteral("current")) {
            if (auto *session = currentSessionState()) {
                m_historyStore.clearMessages(session->id);
            }
        }
    };
    const auto clearLogs = [this](const QString &mode) {
        if (mode == QStringLiteral("all")) {
            m_historyStore.clearAllLogs();
        } else if (mode == QStringLiteral("current")) {
            if (auto *session = currentSessionState()) {
                m_historyStore.clearLogs(session->id);
            }
        }
    };

    clearMessages(clearMessagesOnExit());
    clearLogs(clearLogsOnExit());
}

void AppFacade::notifyCurrentSessionViewsChanged()
{
    refreshSessionsModel();
    emit currentSessionChanged();
}

void AppFacade::notifyCurrentSessionAndSubscriptionsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    emit currentSessionChanged();
    emit subscriptionsChanged();
}

void AppFacade::notifySessionViewsChanged()
{
    refreshSessionsModel();
    emit sessionsChanged();
    emit currentSessionChanged();
}

void AppFacade::notifySessionAndSubscriptionViewsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    emit sessionsChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
}

void AppFacade::notifySelectedSessionViewsChanged()
{
    refreshSubscriptionsModel();
    m_messagesModel.setRows(currentSessionState() ? currentSessionState()->messageRows : QVariantList {});
    m_logsModel.setRows(currentSessionState() ? currentSessionState()->logRows : QVariantList {});
    refreshScriptTestSamplesModel();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit messageStreamChanged();
    emit logStreamChanged();
    emit scriptLibraryChanged();
    emit scriptTestSamplesChanged();
}

void AppFacade::notifySessionCollectionViewsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    m_messagesModel.setRows(currentSessionState() ? currentSessionState()->messageRows : QVariantList {});
    m_logsModel.setRows(currentSessionState() ? currentSessionState()->logRows : QVariantList {});
    refreshScriptsModel();
    refreshScriptTestSamplesModel();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit messageStreamChanged();
    emit logStreamChanged();
    emit scriptLibraryChanged();
}

void AppFacade::copyTextToClipboard(const QString &text) const
{
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}
