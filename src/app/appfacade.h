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
#include "app/appsettingsfacade.h"
#include "app/logstreamfacade.h"
#include "app/scriptlibraryfacade.h"
#include "app/workbenchfacade.h"
#include "services/storage/historystore.h"
#include "models/eventstreammodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/scripttestsamplesmodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"
#include "models/subscriptionlistmodel.h"
#include "services/payload/payloadcodec.h"

class AppFacade : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WorkbenchFacade* workbench READ workbench CONSTANT)
    Q_PROPERTY(AppSettingsFacade* settings READ settings CONSTANT)
    Q_PROPERTY(ScriptLibraryFacade* scriptLibrary READ scriptLibrary CONSTANT)
    Q_PROPERTY(LogStreamFacade* logStream READ logStream CONSTANT)

public:
    explicit AppFacade(QObject *parent = nullptr);
    ~AppFacade() override;

    WorkbenchFacade *workbench();
    AppSettingsFacade *settings();
    ScriptLibraryFacade *scriptLibrary();
    LogStreamFacade *logStream();

signals:
    void sessionsChanged();
    void currentSessionIndexChanged();
    void currentSessionChanged();
    void subscriptionsChanged();
    void messageStreamChanged();
    void logStreamChanged();
    void messageStreamRowAppended(const QVariantMap &row);
    void logStreamRowAppended(const QVariantMap &row);
    void scriptLibraryChanged();
    void scriptTestSamplesChanged();
    void themeModeChanged();
    void effectiveThemeChanged();
    void languageModeChanged();
    void languageChanged();
    void messageRetentionLimitChanged();
    void logRetentionLimitChanged();
    void historyPageSizeChanged();
    void maxIncomingPayloadBytesChanged();
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
    SubscriptionEntry *subscriptionByTopic(SessionState *session, const QString &topic);
    const SubscriptionEntry *subscriptionByTopic(const SessionState *session, const QString &topic) const;
    QString scriptName(const QString &id) const;

    void bindSessionSignals(SessionState *session);
    void configureSession(SessionState &session, const QVariantMap &config, bool keepNameFallback);
    void initializeSessionRuntime(SessionState *session);
    void destroySessionRuntime(SessionState &session);
    void connectSession(SessionState &session, const QString &eventPrefix);
    QSslConfiguration sslConfigurationForSession(const SessionState &session, QString &errorMessage) const;
    void restoreActiveSubscriptions(SessionState &session, bool emitEvents);
    void ensureSubscriptionActive(SessionState &session, SubscriptionEntry &entry, bool emitEvents);
    void updatePublishStatus(
        SessionState &session,
        const QString &state,
        const QString &reason = QString(),
        qint32 messageId = -1);
    void notifyCurrentSessionViewsChanged();
    void notifyCurrentSessionAndSubscriptionsChanged();
    void notifySessionViewsChanged();
    void notifySessionAndSubscriptionViewsChanged();
    void notifySelectedSessionViewsChanged();
    void notifySessionCollectionViewsChanged();
    void appendEvent(SessionState &session, const QString &channel, const QString &message);
    qreal subscriptionFps(const SubscriptionEntry &entry, qint64 nowMs) const;
    bool currentSessionHasActiveSubscriptionFps(qint64 nowMs) const;
    void refreshSubscriptionFps();
    void reloadCurrentSessionHistory();
    void refreshSessionsModel();
    void refreshSubscriptionsModel();
    void refreshScriptsModel();
    void refreshScriptTestSamplesModel();
    void loadScripts();
    void loadSessions();
    bool saveSessions();
    SessionState createDefaultSession(const QString &name);
    void reportStorageError(const QString &message);
    void applyExitCleanup();

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
    std::unique_ptr<WorkbenchFacade> m_workbenchFacade;
    std::unique_ptr<AppSettingsFacade> m_settingsFacade;
    std::unique_ptr<ScriptLibraryFacade> m_scriptLibraryFacade;
    std::unique_ptr<LogStreamFacade> m_logStreamFacade;
    QTimer m_subscriptionFpsRefreshTimer;
    QString m_launchTimestamp;
};
