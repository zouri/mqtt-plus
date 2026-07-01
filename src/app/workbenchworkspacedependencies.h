#pragma once

#include <QObject>
#include <QVariantMap>

#include <functional>

class EventController;
class EventStreamModel;
class MqttController;
class ScriptLibraryModel;
class SessionController;
class SessionListModel;
class SubscriptionController;
class SubscriptionFilterModel;

struct WorkbenchWorkspaceDependencies {
    std::function<void(QObject *, std::function<void()>)> bindCurrentSessionIndexChanged;
    std::function<void(QObject *, std::function<void()>)> bindCurrentSessionChanged;
    std::function<void(QObject *, std::function<void()>)> bindMessageStreamChanged;
    std::function<void(QObject *, std::function<void(const QVariantMap &)>)> bindMessageStreamRowAppended;
    std::function<void(QObject *, std::function<void()>)> bindScriptLibraryChanged;
    SessionController *sessionController = nullptr;
    MqttController *mqttController = nullptr;
    SubscriptionController *subscriptionController = nullptr;
    EventController *eventController = nullptr;
    SessionListModel *sessions = nullptr;
    SubscriptionFilterModel *filteredSubscriptions = nullptr;
    EventStreamModel *messages = nullptr;
    ScriptLibraryModel *scripts = nullptr;
};
