#include "app/applicationcore.h"

#include "app/applicationcoreutils.h"

using namespace ApplicationCoreUtils;

ApplicationCore::ApplicationCore(QObject *parent)
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
    m_sessionController.setCore(this);
    m_filteredSubscriptionsModel.setSourceModel(&m_subscriptionsModel);
    m_launchTimestamp = timestampNow();
    connect(&m_scriptController, &ScriptController::storageError, this, &ApplicationCore::reportStorageError);
    connect(&m_themeController, &ThemeController::modeChanged, this, &ApplicationCore::themeModeChanged);
    connect(&m_themeController, &ThemeController::effectiveThemeChanged, this, &ApplicationCore::effectiveThemeChanged);
    connect(&m_languageController, &LanguageController::modeChanged, this, &ApplicationCore::languageModeChanged);
    connect(&m_languageController, &LanguageController::languageChanged, this, [this]() {
        refreshSessionsModel();
        refreshSubscriptionsModel();
        emit currentSessionChanged();
        emit sessionsChanged();
        emit subscriptionsChanged();
        emit languageChanged();
    });
    connect(&m_preferencesController, &PreferencesController::messageRetentionLimitChanged, this, &ApplicationCore::messageRetentionLimitChanged);
    connect(&m_preferencesController, &PreferencesController::logRetentionLimitChanged, this, &ApplicationCore::logRetentionLimitChanged);
    connect(&m_preferencesController, &PreferencesController::historyPageSizeChanged, this, [this]() {
        reloadCurrentSessionHistory();
        emit messageStreamChanged();
        emit logStreamChanged();
        emit historyPageSizeChanged();
    });
    connect(&m_preferencesController, &PreferencesController::maxIncomingPayloadBytesChanged, this, &ApplicationCore::maxIncomingPayloadBytesChanged);
    connect(&m_preferencesController, &PreferencesController::deleteHistoryWithSessionChanged, this, &ApplicationCore::deleteHistoryWithSessionChanged);
    connect(&m_preferencesController, &PreferencesController::saveMessagesWhenOutputPausedChanged, this, &ApplicationCore::saveMessagesWhenOutputPausedChanged);
    connect(&m_preferencesController, &PreferencesController::clearMessagesOnExitChanged, this, &ApplicationCore::clearMessagesOnExitChanged);
    connect(&m_preferencesController, &PreferencesController::clearLogsOnExitChanged, this, &ApplicationCore::clearLogsOnExitChanged);
    connect(&m_preferencesController, &PreferencesController::windowWidthChanged, this, &ApplicationCore::windowWidthChanged);
    connect(&m_preferencesController, &PreferencesController::windowHeightChanged, this, &ApplicationCore::windowHeightChanged);
    connect(&m_preferencesController, &PreferencesController::windowMaximizedChanged, this, &ApplicationCore::windowMaximizedChanged);

    m_subscriptionFpsRefreshTimer.setInterval(kSubscriptionFpsRefreshIntervalMs);
    connect(
        &m_subscriptionFpsRefreshTimer,
        &QTimer::timeout,
        this,
        &ApplicationCore::refreshSubscriptionFps);
    loadScripts();
    loadSessions();
}

ApplicationCore::~ApplicationCore()
{
    applyExitCleanup();
}

void ApplicationCore::applyExitCleanup()
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

void ApplicationCore::notifyCurrentSessionViewsChanged()
{
    refreshSessionsModel();
    emit currentSessionChanged();
}

void ApplicationCore::notifyCurrentSessionAndSubscriptionsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    emit currentSessionChanged();
    emit subscriptionsChanged();
}

void ApplicationCore::notifySessionViewsChanged()
{
    refreshSessionsModel();
    emit sessionsChanged();
    emit currentSessionChanged();
}

void ApplicationCore::notifySessionAndSubscriptionViewsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    emit sessionsChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
}

void ApplicationCore::notifySelectedSessionViewsChanged()
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

void ApplicationCore::notifySessionCollectionViewsChanged()
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

void ApplicationCore::copyTextToClipboard(const QString &text) const
{
    m_platformActions.copyTextToClipboard(text);
}

HistoryStore &ApplicationCore::historyStore()
{
    return m_historyStore;
}

EventStreamModel &ApplicationCore::messagesModel()
{
    return m_messagesModel;
}

EventStreamModel &ApplicationCore::logsModel()
{
    return m_logsModel;
}

SubscriptionListModel &ApplicationCore::subscriptionsModel()
{
    return m_subscriptionsModel;
}

ScriptTestSamplesModel &ApplicationCore::scriptTestSamplesModel()
{
    return m_scriptTestSamplesModel;
}

ScriptController &ApplicationCore::scriptController()
{
    return m_scriptController;
}

SubscriptionController &ApplicationCore::subscriptionController()
{
    return m_subscriptionController;
}

EventController &ApplicationCore::eventController()
{
    return m_eventController;
}

QTimer &ApplicationCore::subscriptionFpsRefreshTimer()
{
    return m_subscriptionFpsRefreshTimer;
}

QString ApplicationCore::launchTimestamp() const
{
    return m_launchTimestamp;
}

void ApplicationCore::emitSessionsChanged()
{
    emit sessionsChanged();
}

void ApplicationCore::emitSubscriptionsChanged()
{
    emit subscriptionsChanged();
}

void ApplicationCore::emitMessageStreamChanged()
{
    emit messageStreamChanged();
}

void ApplicationCore::emitLogStreamChanged()
{
    emit logStreamChanged();
}

void ApplicationCore::emitMessageStreamRowAppended(const QVariantMap &row)
{
    emit messageStreamRowAppended(row);
}

void ApplicationCore::emitLogStreamRowAppended(const QVariantMap &row)
{
    emit logStreamRowAppended(row);
}

void ApplicationCore::emitScriptTestSamplesChanged()
{
    emit scriptTestSamplesChanged();
}
