#pragma once

#include <QSettings>
#include <QSslConfiguration>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

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
#include "services/storage/historystore.h"
#include "services/scripting/luarunner.h"
#include "models/eventstreammodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/scripttestsamplesmodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionlistmodel.h"
#include "services/payload/payloadcodec.h"

class AppFacade : public QObject
{
    Q_OBJECT
    friend class MqttController;
    friend class SubscriptionController;
    friend class EventController;
    friend class SessionController;
    Q_PROPERTY(SessionListModel* sessions READ sessions CONSTANT)
    Q_PROPERTY(SubscriptionListModel* subscriptions READ subscriptions CONSTANT)
    Q_PROPERTY(EventStreamModel* events READ events CONSTANT)
    Q_PROPERTY(ScriptLibraryModel* scripts READ scripts CONSTANT)
    Q_PROPERTY(ScriptTestSamplesModel* scriptTestSamples READ scriptTestSamples CONSTANT)
    Q_PROPERTY(int currentSessionIndex READ currentSessionIndex WRITE setCurrentSessionIndex NOTIFY currentSessionIndexChanged)
    Q_PROPERTY(QVariantMap currentSession READ currentSession NOTIFY currentSessionChanged)
    Q_PROPERTY(QVariantMap sessionStatus READ sessionStatus NOTIFY currentSessionChanged)
    Q_PROPERTY(QVariantMap publishStatus READ publishStatus NOTIFY currentSessionChanged)
    Q_PROPERTY(QStringList payloadFormats READ payloadFormats CONSTANT)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString effectiveTheme READ effectiveTheme NOTIFY effectiveThemeChanged)
    Q_PROPERTY(QString languageMode READ languageMode WRITE setLanguageMode NOTIFY languageModeChanged)
    Q_PROPERTY(QString effectiveLanguage READ effectiveLanguage NOTIFY languageChanged)
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages NOTIFY languageChanged)

public:
    explicit AppFacade(QObject *parent = nullptr);
    ~AppFacade() override = default;

    SessionListModel *sessions();
    SubscriptionListModel *subscriptions();
    EventStreamModel *events();
    ScriptLibraryModel *scripts();
    ScriptTestSamplesModel *scriptTestSamples();
    int currentSessionIndex() const;
    QVariantMap currentSession() const;
    QVariantMap sessionStatus() const;
    QVariantMap publishStatus() const;
    QStringList payloadFormats() const;
    QString themeMode() const;
    QString effectiveTheme() const;
    QString languageMode() const;
    QString effectiveLanguage() const;
    QVariantList availableLanguages() const;

    void setCurrentSessionIndex(int index);
    void setThemeMode(const QString &mode);
    void setLanguageMode(const QString &mode);

    Q_INVOKABLE QVariantMap defaultSessionConfig() const;
    Q_INVOKABLE QVariantMap sessionConfigAt(int index) const;
    Q_INVOKABLE bool updateSessionConfigAt(int index, const QVariantMap &config);
    Q_INVOKABLE void addSessionWithConfig(const QVariantMap &config);
    Q_INVOKABLE void duplicateSessionAt(int index);
    Q_INVOKABLE void removeSessionAt(int index);
    Q_INVOKABLE void connectCurrentSession();
    Q_INVOKABLE void disconnectCurrentSession();
    Q_INVOKABLE void setCurrentOutputPaused(bool paused);
    Q_INVOKABLE bool upsertCurrentSubscription(
        const QString &topic,
        int qos = 0,
        int format = 0,
        const QString &scriptId = QString(),
        const QString &alias = QString());
    Q_INVOKABLE bool updateCurrentSubscription(const QString &topic, const QString &alias, const QString &scriptId);
    Q_INVOKABLE void removeCurrentSubscription(const QString &topic);
    Q_INVOKABLE void setCurrentSubscriptionPaused(const QString &topic, bool paused);
    Q_INVOKABLE void publishCurrentSession(
        const QString &topic,
        const QString &payload,
        int format = 0,
        int qos = 0,
        bool retain = false);
    Q_INVOKABLE void clearCurrentMessages();
    Q_INVOKABLE int loadOlderCurrentSessionEvents();
    Q_INVOKABLE QString upsertScript(const QString &id, const QString &name, const QString &code);
    Q_INVOKABLE bool deleteScript(const QString &id);
    Q_INVOKABLE QVariantMap testScript(
        const QString &code,
        const QString &topic,
        const QString &payload,
        int format = 0) const;

signals:
    void sessionsChanged();
    void currentSessionIndexChanged();
    void currentSessionChanged();
    void subscriptionsChanged();
    void eventStreamChanged();
    void eventStreamRowAppended(const QVariantMap &row);
    void scriptLibraryChanged();
    void scriptTestSamplesChanged();
    void themeModeChanged();
    void effectiveThemeChanged();
    void languageModeChanged();
    void languageChanged();

private:
    SessionState *currentSessionState();
    const SessionState *currentSessionState() const;
    SessionState *sessionById(const QString &sessionId);
    const SessionState *sessionById(const QString &sessionId) const;
    SubscriptionEntry *subscriptionByTopic(SessionState *session, const QString &topic);
    const SubscriptionEntry *subscriptionByTopic(const SessionState *session, const QString &topic) const;
    const SubscriptionEntry *bestSubscriptionForTopic(const SessionState &session, const QString &topic) const;
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
    void appendRenderedEventRow(SessionState &session, const QVariantMap &row);
    void appendEvent(SessionState &session, const QString &channel, const QString &message);
    void appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes);
    LuaScriptResult parseIncomingPayload(
        const SessionState &session,
        const SubscriptionEntry *subscription,
        const QString &topic,
        const QByteArray &payloadBytes,
        const QString &timestamp,
        QString &scriptNameOut,
        QString &decodedPayloadOut) const;
    qreal subscriptionFps(const SubscriptionEntry &entry, qint64 nowMs) const;
    bool currentSessionHasActiveSubscriptionFps(qint64 nowMs) const;
    void refreshSubscriptionFps();
    void trimVisibleEventRows(SessionState &session);
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

    QSettings m_settings;
    SessionController m_sessionController;
    ScriptController m_scriptController;
    SubscriptionController m_subscriptionController;
    MqttController m_mqttController;
    EventController m_eventController;
    ThemeController m_themeController;
    LanguageController m_languageController;
    HistoryStore m_historyStore;
    SessionListModel m_sessionsModel;
    SubscriptionListModel m_subscriptionsModel;
    EventStreamModel m_eventsModel;
    ScriptLibraryModel m_scriptsModel;
    ScriptTestSamplesModel m_scriptTestSamplesModel;
    QTimer m_subscriptionFpsRefreshTimer;
    QString m_launchTimestamp;
};
