#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <memory>

#include "domain/session.h"
#include "controllers/scriptcontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/subscriptioncontroller.h"
#include "controllers/mqttcontroller.h"
#include "controllers/eventcontroller.h"
#include "controllers/themecontroller.h"
#include "controllers/languagecontroller.h"
#include "controllers/preferencescontroller.h"
#include "models/eventstreammodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/scripttestsamplesmodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"
#include "models/subscriptionlistmodel.h"

class AppModelSync;
class AppControllerWiring;
class AppHistoryServices;
class AppScriptServices;
class AppSurfaceDependencies;
class AppSessionRuntimeBindings;
class AppSessionLifecycle;
class UiEventHub;

class AppRuntimeState : public QObject
{
    Q_OBJECT

public:
    explicit AppRuntimeState(UiEventHub *uiEvents, QObject *parent = nullptr);
    ~AppRuntimeState() override;

    AppSurfaceDependencies *surfaceDependencies();

private:
    SessionState *currentSessionState();
    const SessionState *currentSessionState() const;
    SessionState *sessionById(const QString &sessionId);
    const SessionState *sessionById(const QString &sessionId) const;
    void publishCurrentSessionViewsChanged();
    void publishCurrentSessionAndSubscriptionsChanged();
    void publishSessionViewsChanged();
    void publishSessionAndSubscriptionViewsChanged();
    void publishSelectedSessionViewsChanged();
    void publishSessionCollectionViewsChanged();
    void appendEvent(SessionState &session, const QString &channel, const QString &message);
    void refreshSubscriptionFps();
    void reloadCurrentSessionHistory();
    void loadSessions();
    bool saveSessions();
    void reportStorageError(const QString &message);
    void applyExitCleanup();

    UiEventHub *m_uiEvents = nullptr;
    QSettings m_settings;
    SessionController m_sessionController;
    ScriptController m_scriptController;
    SubscriptionController m_subscriptionController;
    MqttController m_mqttController;
    EventController m_eventController;
    ThemeController m_themeController;
    LanguageController m_languageController;
    PreferencesController m_preferencesController;
    SessionListModel m_sessionsModel;
    SubscriptionListModel m_subscriptionsModel;
    SubscriptionFilterModel m_filteredSubscriptionsModel;
    EventStreamModel m_messagesModel;
    EventStreamModel m_logsModel;
    ScriptLibraryModel m_scriptsModel;
    ScriptTestSamplesModel m_scriptTestSamplesModel;
    std::unique_ptr<AppHistoryServices> m_historyServices;
    std::unique_ptr<AppModelSync> m_modelSync;
    std::unique_ptr<AppScriptServices> m_scriptServices;
    std::unique_ptr<AppSurfaceDependencies> m_surfaceDependencies;
    std::unique_ptr<AppSessionRuntimeBindings> m_sessionRuntimeBindings;
    std::unique_ptr<AppSessionLifecycle> m_sessionLifecycle;
    std::unique_ptr<AppControllerWiring> m_controllerWiring;
};
