#pragma once

#include <QString>

class EventController;
class HistoryStore;
class PreferencesController;
class SessionController;

struct ApplicationExitCleanupDependencies
{
    EventController *eventController = nullptr;
    HistoryStore *historyStore = nullptr;
    PreferencesController *preferencesController = nullptr;
    SessionController *sessionController = nullptr;
};

class ApplicationExitCleanup
{
public:
    void setDependencies(const ApplicationExitCleanupDependencies &dependencies);
    void apply();

private:
    void clearMessages(const QString &mode);
    void clearLogs(const QString &mode);

    ApplicationExitCleanupDependencies m_dependencies;
};
