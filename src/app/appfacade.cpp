#include "app/appfacade.h"

#include "app/appfacadeutils.h"

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
    , m_sessionsModel(this)
    , m_subscriptionsModel(this)
    , m_eventsModel(this)
    , m_scriptsModel(this)
    , m_scriptTestSamplesModel(this)
{
    m_sessionController.setFacade(this);
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

    m_subscriptionFpsRefreshTimer.setInterval(kSubscriptionFpsRefreshIntervalMs);
    connect(
        &m_subscriptionFpsRefreshTimer,
        &QTimer::timeout,
        this,
        &AppFacade::refreshSubscriptionFps);
    loadScripts();
    loadSessions();
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
    m_eventsModel.setRows(currentSessionState() ? currentSessionState()->eventRows : QVariantList {});
    refreshScriptTestSamplesModel();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit eventStreamChanged();
    emit scriptLibraryChanged();
    emit scriptTestSamplesChanged();
}

void AppFacade::notifySessionCollectionViewsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    m_eventsModel.setRows(currentSessionState() ? currentSessionState()->eventRows : QVariantList {});
    refreshScriptsModel();
    refreshScriptTestSamplesModel();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit eventStreamChanged();
    emit scriptLibraryChanged();
}
