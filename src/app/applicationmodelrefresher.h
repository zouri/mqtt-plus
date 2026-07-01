#pragma once

#include "domain/session.h"

class ScriptController;
class ScriptLibraryModel;
class ScriptTestSamplesModel;
class SessionController;
class SessionListModel;
class SubscriptionController;
class SubscriptionListModel;

class ApplicationModelRefresher
{
public:
    ApplicationModelRefresher(
        SessionController &sessionController,
        ScriptController &scriptController,
        SubscriptionController &subscriptionController,
        SessionListModel &sessionsModel,
        SubscriptionListModel &subscriptionsModel,
        ScriptLibraryModel &scriptsModel,
        ScriptTestSamplesModel &scriptTestSamplesModel);

    void refreshSessions();
    void refreshSubscriptions(const SessionState *currentSession);
    void refreshScripts();
    void refreshScriptTestSamples(const SessionState *currentSession);

private:
    SessionController &m_sessionController;
    ScriptController &m_scriptController;
    SubscriptionController &m_subscriptionController;
    SessionListModel &m_sessionsModel;
    SubscriptionListModel &m_subscriptionsModel;
    ScriptLibraryModel &m_scriptsModel;
    ScriptTestSamplesModel &m_scriptTestSamplesModel;
};
