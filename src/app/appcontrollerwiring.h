#pragma once

#include "domain/session.h"
#include "models/subscriptionlistmodel.h"

#include <QByteArray>
#include <QString>
#include <QVariantMap>

#include <functional>

class AppModelSync;
class UiEventHub;
class EventController;
class EventStreamModel;
class HistoryStore;
class LanguageController;
class MqttController;
class PreferencesController;
class ScriptController;
class SessionController;
class SubscriptionController;
class ThemeController;

class AppControllerWiring
{
public:
    struct Dependencies
    {
        EventController *eventController = nullptr;
        SessionController *sessionController = nullptr;
        SubscriptionController *subscriptionController = nullptr;
        MqttController *mqttController = nullptr;
        ScriptController *scriptController = nullptr;
        ThemeController *themeController = nullptr;
        LanguageController *languageController = nullptr;
        PreferencesController *preferencesController = nullptr;
        AppModelSync *modelSync = nullptr;
        UiEventHub *uiEvents = nullptr;
        HistoryStore *historyStore = nullptr;
        EventStreamModel *messagesModel = nullptr;
        EventStreamModel *logsModel = nullptr;
        SubscriptionListModel *subscriptionsModel = nullptr;
        QString launchTimestamp;
        std::function<SessionState *()> currentSession;
        std::function<SessionState *(const QString &)> sessionById;
        std::function<bool()> subscriptionFpsRefreshActive;
        std::function<void()> startSubscriptionFpsRefresh;
        std::function<void(bool)> setSubscriptionFpsRefreshActive;
        std::function<void(SessionState &, const QString &, const QString &)> appendEvent;
        std::function<void()> reloadCurrentSessionHistory;
        std::function<void(const QString &)> reportStorageError;
        std::function<void(SessionState &, const QVariantMap &, bool)> configureSession;
        std::function<void(SessionState &, const QString &, const QString &, qint32)> updatePublishStatus;
        std::function<bool()> saveSessions;
        std::function<void(SessionState &, const QString &)> connectSession;
        std::function<SessionState(const QString &)> createDefaultSession;
        std::function<void(SessionState &)> destroySessionRuntime;
        std::function<void()> publishSelectedSessionViewsChanged;
        std::function<void()> publishCurrentSessionViewsChanged;
        std::function<void()> publishCurrentSessionAndSubscriptionsChanged;
        std::function<void()> publishSessionCollectionViewsChanged;
        std::function<void()> publishSessionViewsChanged;
        std::function<void()> publishSessionAndSubscriptionViewsChanged;
    };

    explicit AppControllerWiring(Dependencies dependencies);

private:
    void bindEventController();
    void bindSessionController();
    void bindSubscriptionController();
    void bindMqttController();
    void bindControllerSignals();

    Dependencies m_dependencies;
};
