#pragma once

#include "app/scriptsbus.h"

#include <QString>
#include <functional>

class AppModelSync;
class ScriptController;
class ScriptLibraryModel;
class ScriptTestSamplesModel;
class SessionController;

class AppScriptServices
{
public:
    struct Dependencies
    {
        ScriptController *scriptController = nullptr;
        SessionController *sessionController = nullptr;
        ScriptLibraryModel *scriptsModel = nullptr;
        ScriptTestSamplesModel *scriptTestSamplesModel = nullptr;
        AppModelSync *modelSync = nullptr;
        std::function<bool()> saveSessions;
        std::function<void()> publishCurrentSessionAndSubscriptionsChanged;
        std::function<void()> publishSessionAndSubscriptionViewsChanged;
        std::function<void()> publishScriptsChanged;
    };

    explicit AppScriptServices(Dependencies dependencies);

    void loadScripts();
    bool scriptExists(const QString &scriptId) const;
    ScriptsBus::Dependencies scriptsBusDependencies();

private:
    Dependencies m_dependencies;
};
