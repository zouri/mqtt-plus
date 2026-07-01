#pragma once

#include <QString>
#include <QVariantMap>

class ApplicationModelRefresher;
class ApplicationNotifier;
class EventController;
class EventStreamModel;
class SessionController;

struct ApplicationViewRefreshDependencies
{
    ApplicationNotifier *notifier = nullptr;
    ApplicationModelRefresher *modelRefresher = nullptr;
    SessionController *sessionController = nullptr;
    EventController *eventController = nullptr;
    EventStreamModel *messagesModel = nullptr;
    EventStreamModel *logsModel = nullptr;
};

class ApplicationViewRefreshCoordinator
{
public:
    void setDependencies(const ApplicationViewRefreshDependencies &dependencies);

    void refreshSessionsModel();
    void refreshSubscriptionsModel();
    void refreshScriptsModel();
    void refreshScriptTestSamplesModel();

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
    ApplicationViewRefreshDependencies m_dependencies;
};
