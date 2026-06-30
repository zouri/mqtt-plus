#pragma once

#include <QSettings>
#include <QSslConfiguration>
#include <QPointF>
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
#include "controllers/applicationcontext.h"
#include "controllers/scriptcontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/subscriptioncontroller.h"
#include "controllers/mqttcontroller.h"
#include "controllers/eventcontroller.h"
#include "controllers/themecontroller.h"
#include "controllers/languagecontroller.h"
#include "controllers/preferencescontroller.h"
#include "services/storage/historystore.h"
#include "services/scripting/luarunner.h"
#include "models/eventstreammodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/scripttestsamplesmodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"
#include "models/subscriptionlistmodel.h"
#include "platform/platformactions.h"
#include "services/payload/payloadcodec.h"

class ApplicationCore
    : public QObject
    , public SessionControllerContext
    , public MqttControllerContext
    , public EventControllerContext
    , public SubscriptionControllerContext
{
    Q_OBJECT

public:
    explicit ApplicationCore(QObject *parent = nullptr);
    ~ApplicationCore() override;

    SessionListModel *sessions();
    SubscriptionListModel *subscriptions();
    SubscriptionFilterModel *filteredSubscriptions();
    EventStreamModel *messages();
    EventStreamModel *logs();
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
    int messageRetentionLimit() const override;
    int logRetentionLimit() const override;
    int historyPageSize() const override;
    int maxIncomingPayloadBytes() const override;
    bool deleteHistoryWithSession() const override;
    bool saveMessagesWhenOutputPaused() const override;
    QString clearMessagesOnExit() const;
    QString clearLogsOnExit() const;
    int windowWidth() const;
    int windowHeight() const;
    bool windowMaximized() const;

    void setCurrentSessionIndex(int index);
    void setThemeMode(const QString &mode);
    void setLanguageMode(const QString &mode);
    void setMessageRetentionLimit(int limit);
    void setLogRetentionLimit(int limit);
    void setHistoryPageSize(int pageSize);
    void setMaxIncomingPayloadBytes(int bytes);
    void setDeleteHistoryWithSession(bool enabled);
    void setSaveMessagesWhenOutputPaused(bool enabled);
    void setClearMessagesOnExit(const QString &mode);
    void setClearLogsOnExit(const QString &mode);
    void setWindowMaximized(bool maximized);

    QVariantMap defaultSessionConfig() const;
    QVariantMap sessionConfigAt(int index) const;
    bool updateSessionConfigAt(int index, const QVariantMap &config);
    void addSessionWithConfig(const QVariantMap &config);
    void duplicateSessionAt(int index);
    void removeSessionAt(int index);
    QString showSessionContextMenu(int index, const QPointF &globalPosition);
    QString showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition);
    void connectCurrentSession();
    void disconnectCurrentSession();
    void setCurrentOutputPaused(bool paused);
    bool upsertCurrentSubscription(
        const QString &topic,
        int qos = 0,
        int format = 0,
        const QString &scriptId = QString(),
        const QString &alias = QString());
    bool updateCurrentSubscription(
        const QString &topic,
        const QString &newTopic,
        const QString &alias,
        const QString &scriptId);
    void removeCurrentSubscription(const QString &topic);
    void setCurrentSubscriptionPaused(const QString &topic, bool paused);
    void publishCurrentSession(
        const QString &topic,
        const QString &payload,
        int format = 0,
        int qos = 0,
        bool retain = false);
    void copyTextToClipboard(const QString &text) const;
    void clearCurrentMessages();
    void clearCurrentLogs();
    void clearAllMessages();
    void clearAllLogs();
    void clearAllHistory();
    int loadOlderCurrentSessionMessages();
    int loadOlderCurrentSessionLogs();
    QString upsertScript(
        const QString &id,
        const QString &name,
        const QString &description,
        const QString &code);
    bool deleteScript(const QString &id);
    QVariantMap testScript(
        const QString &code,
        const QString &topic,
        const QString &payload,
        int format = 0) const;
    void saveWindowGeometry(int width, int height);

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
    SessionState *currentSessionState() override;
    const SessionState *currentSessionState() const override;
    SessionState *sessionById(const QString &sessionId) override;
    const SessionState *sessionById(const QString &sessionId) const override;
    SubscriptionEntry *subscriptionByTopic(SessionState *session, const QString &topic);
    const SubscriptionEntry *subscriptionByTopic(const SessionState *session, const QString &topic) const;
    const SubscriptionEntry *bestSubscriptionForTopic(const SessionState &session, const QString &topic) const;
    QString scriptName(const QString &id) const override;

    HistoryStore &historyStore() override;
    EventStreamModel &messagesModel() override;
    EventStreamModel &logsModel() override;
    SubscriptionListModel &subscriptionsModel() override;
    ScriptTestSamplesModel &scriptTestSamplesModel() override;
    ScriptController &scriptController() override;
    SubscriptionController &subscriptionController() override;
    EventController &eventController() override;
    QTimer &subscriptionFpsRefreshTimer() override;
    QString launchTimestamp() const override;

    void bindSessionSignals(SessionState *session);
    void configureSession(SessionState &session, const QVariantMap &config, bool keepNameFallback) override;
    void initializeSessionRuntime(SessionState *session) override;
    void destroySessionRuntime(SessionState &session) override;
    void connectSession(SessionState &session, const QString &eventPrefix) override;
    QSslConfiguration sslConfigurationForSession(const SessionState &session, QString &errorMessage) const override;
    void restoreActiveSubscriptions(SessionState &session, bool emitEvents);
    void ensureSubscriptionActive(SessionState &session, SubscriptionEntry &entry, bool emitEvents);
    void updatePublishStatus(
        SessionState &session,
        const QString &state,
        const QString &reason = QString(),
        qint32 messageId = -1) override;
    void notifyCurrentSessionViewsChanged() override;
    void notifyCurrentSessionAndSubscriptionsChanged() override;
    void notifySessionViewsChanged() override;
    void notifySessionAndSubscriptionViewsChanged() override;
    void notifySelectedSessionViewsChanged() override;
    void notifySessionCollectionViewsChanged() override;
    void appendRenderedMessageRow(SessionState &session, const QVariantMap &row);
    void appendRenderedLogRow(SessionState &session, const QVariantMap &row);
    void appendEvent(SessionState &session, const QString &channel, const QString &message) override;
    void appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes) override;
    LuaScriptResult parseIncomingPayload(
        const SessionState &session,
        const SubscriptionEntry *subscription,
        const QString &topic,
        const QByteArray &payloadBytes,
        const QString &timestamp,
        QString &scriptNameOut,
        QString &decodedPayloadOut) const override;
    qreal subscriptionFps(const SubscriptionEntry &entry, qint64 nowMs) const override;
    bool currentSessionHasActiveSubscriptionFps(qint64 nowMs) const override;
    void refreshSubscriptionFps();
    void trimVisibleMessageRows(SessionState &session);
    void trimVisibleLogRows(SessionState &session);
    void reloadCurrentSessionHistory() override;
    void refreshSessionsModel() override;
    void refreshSubscriptionsModel() override;
    void refreshScriptsModel();
    void refreshScriptTestSamplesModel() override;
    void loadScripts();
    void loadSessions();
    bool saveSessions() override;
    SessionState createDefaultSession(const QString &name) override;
    void reportStorageError(const QString &message);
    void applyExitCleanup();
    void emitSessionsChanged() override;
    void emitSubscriptionsChanged() override;
    void emitMessageStreamChanged() override;
    void emitLogStreamChanged() override;
    void emitMessageStreamRowAppended(const QVariantMap &row) override;
    void emitLogStreamRowAppended(const QVariantMap &row) override;
    void emitScriptTestSamplesChanged() override;

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
    PlatformActions m_platformActions;
    QTimer m_subscriptionFpsRefreshTimer;
    QString m_launchTimestamp;
};
