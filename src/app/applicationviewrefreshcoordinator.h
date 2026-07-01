#pragma once

#include <QString>
#include <QVariantMap>

class ApplicationCore;
class EventController;
class EventStreamModel;
class ScriptController;
class ScriptLibraryModel;
class ScriptTestSamplesModel;
class SessionController;
class SessionListModel;
class SubscriptionListModel;

struct ApplicationViewRefreshDependencies
{
    ApplicationCore *core = nullptr;
    SessionController *sessionController = nullptr;
    EventController *eventController = nullptr;
    ScriptController *scriptController = nullptr;
    SessionListModel *sessionsModel = nullptr;
    SubscriptionListModel *subscriptionsModel = nullptr;
    EventStreamModel *messagesModel = nullptr;
    EventStreamModel *logsModel = nullptr;
    ScriptLibraryModel *scriptsModel = nullptr;
    ScriptTestSamplesModel *scriptTestSamplesModel = nullptr;
};

class ApplicationViewRefreshCoordinator
{
public:
    void setDependencies(const ApplicationViewRefreshDependencies &dependencies);

    void refreshSessionsModel();
    void refreshSubscriptionsModel();
    void refreshScriptsModel();
    void refreshScriptTestSamplesModel();

    void reloadCurrentSessionHistory();

    void notifyCurrentSessionViewsChanged();
    void notifyCurrentSessionAndSubscriptionsChanged();
    void notifySessionViewsChanged();
    void notifySessionAndSubscriptionViewsChanged();
    void notifySelectedSessionViewsChanged();
    void notifySessionCollectionViewsChanged();
    void notifyLanguageChanged();
    void notifyHistoryPageSizeChanged();

    void reportStorageError(const QString &message);

    void emitSessionsChanged();
    void emitSubscriptionsChanged();
    void emitMessageStreamChanged();
    void emitLogStreamChanged();
    void emitMessageStreamRowAppended(const QVariantMap &row);
    void emitLogStreamRowAppended(const QVariantMap &row);

private:
    ApplicationViewRefreshDependencies m_deps;
};
