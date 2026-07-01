#pragma once

#include "controllers/eventcontrollercontext.h"
#include "controllers/mqttcontrollercontext.h"
#include "controllers/sessioncontrollercontext.h"
#include "controllers/subscriptioncontrollercontext.h"

#include <QString>

class ApplicationSessionRepository;
class ApplicationSessionRuntime;
class ApplicationViewRefreshCoordinator;
class EventController;
class EventStreamModel;
class HistoryStore;
class MqttController;
class PreferencesController;
class ScriptController;
class ScriptTestSamplesModel;
class SessionController;
class SubscriptionController;
class SubscriptionListModel;
class QTimer;

struct ApplicationSessionControllerContextDependencies
{
    ApplicationSessionRuntime *sessionRuntime = nullptr;
    ApplicationSessionRepository *sessionRepository = nullptr;
    ApplicationViewRefreshCoordinator *viewRefreshCoordinator = nullptr;
    SubscriptionController *subscriptionController = nullptr;
    MqttController *mqttController = nullptr;
    EventController *eventController = nullptr;
    PreferencesController *preferencesController = nullptr;
    HistoryStore *historyStore = nullptr;
    QTimer *subscriptionFpsRefreshTimer = nullptr;
};

struct ApplicationMqttControllerContextDependencies
{
    ApplicationViewRefreshCoordinator *viewRefreshCoordinator = nullptr;
    SessionController *sessionController = nullptr;
    SubscriptionController *subscriptionController = nullptr;
    MqttController *mqttController = nullptr;
    EventController *eventController = nullptr;
};

struct ApplicationEventControllerContextDependencies
{
    ApplicationViewRefreshCoordinator *viewRefreshCoordinator = nullptr;
    SessionController *sessionController = nullptr;
    ScriptController *scriptController = nullptr;
    SubscriptionController *subscriptionController = nullptr;
    EventController *eventController = nullptr;
    PreferencesController *preferencesController = nullptr;
    HistoryStore *historyStore = nullptr;
    EventStreamModel *messagesModel = nullptr;
    EventStreamModel *logsModel = nullptr;
    ScriptTestSamplesModel *scriptTestSamplesModel = nullptr;
    QTimer *subscriptionFpsRefreshTimer = nullptr;
    QString *launchTimestamp = nullptr;
};

struct ApplicationSubscriptionControllerContextDependencies
{
    ApplicationSessionRepository *sessionRepository = nullptr;
    ApplicationViewRefreshCoordinator *viewRefreshCoordinator = nullptr;
    SessionController *sessionController = nullptr;
    ScriptController *scriptController = nullptr;
    SubscriptionController *subscriptionController = nullptr;
    EventController *eventController = nullptr;
    SubscriptionListModel *subscriptionsModel = nullptr;
    QTimer *subscriptionFpsRefreshTimer = nullptr;
};

struct ApplicationControllerContextsDependencies
{
    ApplicationSessionControllerContextDependencies session;
    ApplicationMqttControllerContextDependencies mqtt;
    ApplicationEventControllerContextDependencies event;
    ApplicationSubscriptionControllerContextDependencies subscription;
};

class ApplicationSessionControllerContextAdapter : public SessionControllerContext
{
public:
    explicit ApplicationSessionControllerContextAdapter(ApplicationSessionControllerContextDependencies *dependencies);

    HistoryStore &historyStore() override;
    SubscriptionController &subscriptionController() override;
    QTimer &subscriptionFpsRefreshTimer() override;
    bool deleteHistoryWithSession() const override;
    bool saveSessions() override;
    void configureSession(SessionState &session, const QVariantMap &config, bool keepNameFallback) override;
    void initializeSessionRuntime(SessionState *session) override;
    void destroySessionRuntime(SessionState &session) override;
    void connectSession(SessionState &session, const QString &eventPrefix) override;
    SessionState createDefaultSession(const QString &name) override;
    void updatePublishStatus(
        SessionState &session,
        const QString &state,
        const QString &reason = QString(),
        qint32 messageId = -1) override;
    void reloadCurrentSessionHistory() override;
    void notifyCurrentSessionViewsChanged() override;
    void notifyCurrentSessionAndSubscriptionsChanged() override;
    void notifySelectedSessionViewsChanged() override;
    void notifySessionCollectionViewsChanged() override;
    void refreshSessionsModel() override;
    void emitSessionsChanged() override;
    void emitMessageStreamChanged() override;

private:
    ApplicationSessionControllerContextDependencies *m_dependencies = nullptr;
};

class ApplicationMqttControllerContextAdapter : public MqttControllerContext
{
public:
    explicit ApplicationMqttControllerContextAdapter(ApplicationMqttControllerContextDependencies *dependencies);

    SessionState *currentSessionState() override;
    SessionState *sessionById(const QString &sessionId) override;
    const SessionState *sessionById(const QString &sessionId) const override;
    SubscriptionController &subscriptionController() override;
    EventController &eventController() override;
    void updatePublishStatus(
        SessionState &session,
        const QString &state,
        const QString &reason = QString(),
        qint32 messageId = -1) override;
    void appendEvent(SessionState &session, const QString &channel, const QString &message) override;
    QSslConfiguration sslConfigurationForSession(const SessionState &session, QString &errorMessage) const override;
    void notifyCurrentSessionViewsChanged() override;
    void notifySessionViewsChanged() override;
    void notifySessionAndSubscriptionViewsChanged() override;

private:
    ApplicationMqttControllerContextDependencies *m_dependencies = nullptr;
};

class ApplicationEventControllerContextAdapter : public EventControllerContext
{
public:
    explicit ApplicationEventControllerContextAdapter(ApplicationEventControllerContextDependencies *dependencies);

    SessionState *currentSessionState() override;
    SessionState *sessionById(const QString &sessionId) override;
    HistoryStore &historyStore() override;
    EventStreamModel &messagesModel() override;
    EventStreamModel &logsModel() override;
    ScriptTestSamplesModel &scriptTestSamplesModel() override;
    ScriptController &scriptController() override;
    SubscriptionController &subscriptionController() override;
    QTimer &subscriptionFpsRefreshTimer() override;
    QString launchTimestamp() const override;
    int historyPageSize() const override;
    int messageRetentionLimit() const override;
    int logRetentionLimit() const override;
    int maxIncomingPayloadBytes() const override;
    bool saveMessagesWhenOutputPaused() const override;
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
    QString scriptName(const QString &id) const override;
    void refreshSubscriptionsModel() override;
    void refreshScriptTestSamplesModel() override;
    void emitSubscriptionsChanged() override;
    void emitMessageStreamChanged() override;
    void emitLogStreamChanged() override;
    void emitMessageStreamRowAppended(const QVariantMap &row) override;
    void emitLogStreamRowAppended(const QVariantMap &row) override;

private:
    ApplicationEventControllerContextDependencies *m_dependencies = nullptr;
};

class ApplicationSubscriptionControllerContextAdapter : public SubscriptionControllerContext
{
public:
    explicit ApplicationSubscriptionControllerContextAdapter(ApplicationSubscriptionControllerContextDependencies *dependencies);

    SessionState *currentSessionState() override;
    const SessionState *currentSessionState() const override;
    SessionState *sessionById(const QString &sessionId) override;
    SubscriptionListModel &subscriptionsModel() override;
    ScriptController &scriptController() override;
    QTimer &subscriptionFpsRefreshTimer() override;
    bool saveSessions() override;
    void appendEvent(SessionState &session, const QString &channel, const QString &message) override;
    qreal subscriptionFps(const SubscriptionEntry &entry, qint64 nowMs) const override;
    bool currentSessionHasActiveSubscriptionFps(qint64 nowMs) const override;
    void refreshSubscriptionsModel() override;
    void notifyCurrentSessionAndSubscriptionsChanged() override;
    void notifySessionAndSubscriptionViewsChanged() override;
    void emitSubscriptionsChanged() override;

private:
    ApplicationSubscriptionControllerContextDependencies *m_dependencies = nullptr;
};

class ApplicationControllerContexts
{
public:
    ApplicationControllerContexts();

    void setDependencies(const ApplicationControllerContextsDependencies &dependencies);

    SessionControllerContext &session();
    MqttControllerContext &mqtt();
    EventControllerContext &event();
    SubscriptionControllerContext &subscription();

    SessionState *sessionById(const QString &sessionId);
    void appendEvent(SessionState &session, const QString &channel, const QString &message);
    void reportStorageError(const QString &message);
    void reloadCurrentSessionHistory();
    void notifyCurrentSessionAndSubscriptionsChanged();
    void notifySessionViewsChanged();
    void notifySessionCollectionViewsChanged();

private:
    ApplicationControllerContextsDependencies m_dependencies;
    ApplicationSessionControllerContextAdapter m_session;
    ApplicationMqttControllerContextAdapter m_mqtt;
    ApplicationEventControllerContextAdapter m_event;
    ApplicationSubscriptionControllerContextAdapter m_subscription;
};
