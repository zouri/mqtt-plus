#pragma once

#include <QSettings>
#include <QSslConfiguration>
#include <QPointF>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <memory>

#include <QMqttClient>
#include <QMqttSubscription>

#include "domain/script.h"
#include "domain/session.h"
#include "domain/subscription.h"
#include "controllers/scriptcontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/subscriptioncontroller.h"
#include "controllers/mqttcontroller.h"
#include "controllers/eventcontroller.h"
#include "controllers/themecontroller.h"
#include "controllers/languagecontroller.h"
#include "controllers/preferencescontroller.h"
#include "app/settingsbus.h"
#include "app/logsbus.h"
#include "app/scriptsbus.h"
#include "app/workbenchbus.h"
#include "services/storage/historystore.h"
#include "models/eventstreammodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/scripttestsamplesmodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"
#include "models/subscriptionlistmodel.h"
#include "services/payload/payloadcodec.h"

class AppEventBus;
class ModelCoordinator;
class SessionLifecycleService;

class AppFacade : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WorkbenchBus* workbench READ workbench CONSTANT)
    Q_PROPERTY(SettingsBus* settings READ settings CONSTANT)
    Q_PROPERTY(ScriptsBus* scripts READ scripts CONSTANT)
    Q_PROPERTY(LogsBus* logs READ logs CONSTANT)

public:
    explicit AppFacade(QObject *parent = nullptr);
    ~AppFacade() override;

    void setEventBus(AppEventBus *eventBus);

    WorkbenchBus *workbench();
    SettingsBus *settings();
    ScriptsBus *scripts();
    LogsBus *logs();

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
    SessionState *currentSessionState();
    const SessionState *currentSessionState() const;
    SessionState *sessionById(const QString &sessionId);
    const SessionState *sessionById(const QString &sessionId) const;
    void bindSessionSignals(SessionState *session);
    void configureSession(SessionState &session, const QVariantMap &config, bool keepNameFallback);
    void initializeSessionRuntime(SessionState *session);
    void destroySessionRuntime(SessionState &session);
    void connectSession(SessionState &session, const QString &eventPrefix);
    QSslConfiguration sslConfigurationForSession(const SessionState &session, QString &errorMessage) const;
    void updatePublishStatus(
        SessionState &session,
        const QString &state,
        const QString &reason = QString(),
        qint32 messageId = -1);
    void publishCurrentSessionViewsChanged();
    void publishCurrentSessionAndSubscriptionsChanged();
    void publishSessionViewsChanged();
    void publishSessionAndSubscriptionViewsChanged();
    void publishSelectedSessionViewsChanged();
    void publishSessionCollectionViewsChanged();
    void appendEvent(SessionState &session, const QString &channel, const QString &message);
    void refreshSubscriptionFps();
    void reloadCurrentSessionHistory();
    void loadScripts();
    void loadSessions();
    bool saveSessions();
    SessionState createDefaultSession(const QString &name);
    void reportStorageError(const QString &message);
    void applyExitCleanup();
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

    QSettings m_settings;
    SessionController m_sessionController;
    ScriptController m_scriptController;
    SubscriptionController m_subscriptionController;
    MqttController m_mqttController;
    EventController m_eventController;
    ThemeController m_themeController;
    LanguageController m_languageController;
    PreferencesController m_preferencesController;
    HistoryStore m_historyStore;
    SessionListModel m_sessionsModel;
    SubscriptionListModel m_subscriptionsModel;
    SubscriptionFilterModel m_filteredSubscriptionsModel;
    EventStreamModel m_messagesModel;
    EventStreamModel m_logsModel;
    ScriptLibraryModel m_scriptsModel;
    ScriptTestSamplesModel m_scriptTestSamplesModel;
    std::unique_ptr<ModelCoordinator> m_modelCoordinator;
    std::unique_ptr<SessionLifecycleService> m_sessionLifecycleService;
    std::unique_ptr<WorkbenchBus> m_workbenchBus;
    std::unique_ptr<SettingsBus> m_settingsBus;
    std::unique_ptr<ScriptsBus> m_scriptsBus;
    std::unique_ptr<LogsBus> m_logsBus;
    AppEventBus *m_eventBus = nullptr;
    QTimer m_subscriptionFpsRefreshTimer;
    QString m_launchTimestamp;
};
