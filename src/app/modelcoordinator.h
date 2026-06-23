#pragma once

#include <functional>

class EventStreamModel;
class ScriptController;
class ScriptLibraryModel;
class ScriptTestSamplesModel;
class SessionController;
class SessionListModel;
struct SessionState;
class SubscriptionController;
class SubscriptionListModel;

class ModelCoordinator
{
public:
    struct Dependencies
    {
        SessionController *sessionController = nullptr;
        SubscriptionController *subscriptionController = nullptr;
        ScriptController *scriptController = nullptr;
        SessionListModel *sessionsModel = nullptr;
        SubscriptionListModel *subscriptionsModel = nullptr;
        EventStreamModel *messagesModel = nullptr;
        EventStreamModel *logsModel = nullptr;
        ScriptLibraryModel *scriptsModel = nullptr;
        ScriptTestSamplesModel *scriptTestSamplesModel = nullptr;
        std::function<SessionState *()> currentSession;
    };

    explicit ModelCoordinator(Dependencies dependencies);

    void refreshSessionsModel();
    void refreshSubscriptionsModel();
    void refreshScriptsModel();
    void refreshScriptTestSamplesModel();
    void syncSelectedSessionModels();
    void syncSessionCollectionModels();

private:
    void syncCurrentSessionEventModels();

    Dependencies m_dependencies;
};
