#include "app/appscriptservices.h"

#include "app/appmodelsync.h"
#include "controllers/scriptcontroller.h"

#include <QtGlobal>

#include <utility>

AppScriptServices::AppScriptServices(Dependencies dependencies)
    : m_dependencies(std::move(dependencies))
{
    Q_ASSERT(m_dependencies.scriptController);
    Q_ASSERT(m_dependencies.sessionController);
    Q_ASSERT(m_dependencies.scriptsModel);
    Q_ASSERT(m_dependencies.scriptTestSamplesModel);
    Q_ASSERT(m_dependencies.modelSync);
}

void AppScriptServices::loadScripts()
{
    m_dependencies.scriptController->loadScripts();
    m_dependencies.modelSync->refreshScriptsModel();
}

bool AppScriptServices::scriptExists(const QString &scriptId) const
{
    return m_dependencies.scriptController->scriptById(scriptId) != nullptr;
}

ScriptsBus::Dependencies AppScriptServices::scriptsBusDependencies()
{
    return ScriptsBus::Dependencies {
        .scriptsModel = m_dependencies.scriptsModel,
        .scriptTestSamplesModel = m_dependencies.scriptTestSamplesModel,
        .scriptController = m_dependencies.scriptController,
        .sessionController = m_dependencies.sessionController,
        .syncScriptsModel = [this]() {
            m_dependencies.modelSync->refreshScriptsModel();
        },
        .saveSessions = m_dependencies.saveSessions,
        .publishCurrentSessionAndSubscriptionsChanged = m_dependencies.publishCurrentSessionAndSubscriptionsChanged,
        .publishSessionAndSubscriptionViewsChanged = m_dependencies.publishSessionAndSubscriptionViewsChanged,
        .publishScriptsChanged = m_dependencies.publishScriptsChanged,
    };
}
