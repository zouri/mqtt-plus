#pragma once

#include "domain/session.h"
#include "services/storage/historystore.h"

#include <functional>

class EventController;
class PreferencesController;

class AppHistoryServices
{
public:
    struct Dependencies
    {
        EventController *eventController = nullptr;
        PreferencesController *preferencesController = nullptr;
        std::function<SessionState *()> currentSession;
    };

    explicit AppHistoryServices(Dependencies dependencies);

    HistoryStore *historyStore();
    const HistoryStore *historyStore() const;

    void applyExitCleanup();

private:
    Dependencies m_dependencies;
    HistoryStore m_historyStore;
};
