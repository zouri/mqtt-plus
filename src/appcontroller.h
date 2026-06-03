#pragma once

#include <QPointer>
#include <QSettings>
#include <QSslConfiguration>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <QMqttClient>
#include <QMqttSubscription>

#include "historystore.h"
#include "luarunner.h"
#include "payloadcodec.h"

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList sessionsModel READ sessionsModel NOTIFY sessionsChanged)
    Q_PROPERTY(int currentSessionIndex READ currentSessionIndex WRITE setCurrentSessionIndex NOTIFY currentSessionIndexChanged)
    Q_PROPERTY(QVariantMap currentSession READ currentSession NOTIFY currentSessionChanged)
    Q_PROPERTY(QVariantMap sessionStatus READ sessionStatus NOTIFY currentSessionChanged)
    Q_PROPERTY(QVariantList subscriptionsModel READ subscriptionsModel NOTIFY subscriptionsChanged)
    Q_PROPERTY(QVariantMap publishStatus READ publishStatus NOTIFY currentSessionChanged)
    Q_PROPERTY(QVariantList eventStreamModel READ eventStreamModel NOTIFY eventStreamChanged)
    Q_PROPERTY(QStringList payloadFormats READ payloadFormats CONSTANT)
    Q_PROPERTY(QVariantList scriptLibraryModel READ scriptLibraryModel NOTIFY scriptLibraryChanged)
    Q_PROPERTY(QVariantList scriptTestSamplesModel READ scriptTestSamplesModel NOTIFY scriptTestSamplesChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString effectiveTheme READ effectiveTheme NOTIFY effectiveThemeChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override = default;

    QVariantList sessionsModel() const;
    int currentSessionIndex() const;
    QVariantMap currentSession() const;
    QVariantMap sessionStatus() const;
    QVariantList subscriptionsModel() const;
    QVariantMap publishStatus() const;
    QVariantList eventStreamModel() const;
    QStringList payloadFormats() const;
    QVariantList scriptLibraryModel() const;
    QVariantList scriptTestSamplesModel() const;
    QString themeMode() const;
    QString effectiveTheme() const;

    void setCurrentSessionIndex(int index);
    void setThemeMode(const QString &mode);

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
    Q_INVOKABLE QVariantList loadOlderCurrentEventRows();
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

public:
    struct SubscriptionEntry {
        QString topic;
        QString alias;
        int requestedQos = 0;
        int grantedQos = -1;
        int format = 0;
        QString scriptId;
        bool paused = false;
        QString runtimeState = QStringLiteral("saved");
        QString lastError;
        QPointer<QMqttSubscription> runtimeSubscription;
        QVector<qint64> recentMessageTimestampsMs;
    };

    struct ScriptEntry {
        QString id;
        QString name;
        QString code;
        QString updatedAt;
        QString fileName;
    };

    struct SessionState {
        QString id;
        QString name;
        QString transport = QStringLiteral("tcp");
        int protocolVersion = 5;
        bool sslSecure = true;
        QString alpn;
        QString certificateType = QStringLiteral("ca");
        QString caFile;
        QString clientCertificateFile;
        QString clientKeyFile;
        int connectTimeoutSeconds = 10;
        quint32 sessionExpiryInterval = 0;
        quint16 receiveMaximum = 0;
        quint32 maximumPacketSize = 0;
        quint16 topicAliasMaximum = 0;
        bool requestResponseInformation = false;
        bool requestProblemInformation = false;
        QString authenticationMethod;
        QString authenticationData;
        bool outputPaused = false;
        bool disconnectRequested = false;
        bool sessionRestored = false;
        QString lastError;
        QString brokerInfo;
        QVector<SubscriptionEntry> subscriptions;
        QHash<QString, int> subscriptionFormats;
        QVariantMap publishStatus;
        QVariantList eventRows;
        qint64 oldestLoadedEventId = 0;
        bool loadedAllEventHistory = false;
        QMqttClient *client = nullptr;
        QTimer *connectTimeoutTimer = nullptr;
    };

private:
    SessionState *currentSessionState();
    const SessionState *currentSessionState() const;
    SessionState *sessionById(const QString &sessionId);
    const SessionState *sessionById(const QString &sessionId) const;
    SubscriptionEntry *subscriptionByTopic(SessionState *session, const QString &topic);
    const SubscriptionEntry *subscriptionByTopic(const SessionState *session, const QString &topic) const;
    const SubscriptionEntry *bestSubscriptionForTopic(const SessionState &session, const QString &topic) const;
    const ScriptEntry *scriptById(const QString &id) const;
    QString scriptName(const QString &id) const;

    void bindSessionSignals(SessionState *session);
    void configureSession(SessionState &session, const QVariantMap &config, bool keepNameFallback);
    void initializeSessionRuntime(SessionState *session);
    void destroySessionRuntime(SessionState &session);
    void connectSession(SessionState &session, const QString &eventPrefix);
    QSslConfiguration sslConfigurationForSession(const SessionState &session, QString &errorMessage) const;
    void restoreActiveSubscriptions(SessionState &session, bool emitEvents);
    void ensureSubscriptionActive(SessionState &session, SubscriptionEntry &entry, bool emitEvents);
    void observeSubscription(SessionState &session, SubscriptionEntry &entry, QMqttSubscription *subscription);
    void updateSubscriptionState(
        const QString &sessionId,
        const QString &topic,
        QMqttSubscription::SubscriptionState state);
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
    void loadScripts();
    bool saveScripts();
    void loadSessions();
    bool saveSessions();
    SessionState createDefaultSession(const QString &name);
    void reportStorageError(const QString &message);
    void refreshSystemColorScheme();

    QVector<SessionState> m_sessions;
    QVector<ScriptEntry> m_scripts;
    int m_currentSessionIndex = -1;
    QSettings m_settings;
    HistoryStore m_historyStore;
    QTimer m_subscriptionFpsRefreshTimer;
    QString m_themeMode = QStringLiteral("system");
    bool m_systemDarkMode = false;
    QString m_launchTimestamp;
    bool m_scriptIndexWritable = true;
};
