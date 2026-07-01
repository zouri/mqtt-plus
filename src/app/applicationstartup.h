#pragma once

class ApplicationControllerContexts;
class ApplicationModelRefresher;
class ApplicationSessionRepository;
class ScriptController;
class SessionController;

struct ApplicationStartupDependencies
{
    ApplicationControllerContexts *controllerContexts = nullptr;
    ApplicationModelRefresher *modelRefresher = nullptr;
    ApplicationSessionRepository *sessionRepository = nullptr;
    ScriptController *scriptController = nullptr;
    SessionController *sessionController = nullptr;
};

class ApplicationStartup
{
public:
    void setDependencies(const ApplicationStartupDependencies &dependencies);
    void run();

private:
    void loadScripts();
    void loadSessions();

    ApplicationStartupDependencies m_dependencies;
};
