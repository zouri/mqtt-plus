#include "app/applicationstartup.h"

#include "app/applicationcontrollercontexts.h"
#include "app/applicationmodelrefresher.h"
#include "app/applicationsessionrepository.h"
#include "controllers/scriptcontroller.h"
#include "controllers/sessioncontroller.h"

#include <QString>

void ApplicationStartup::setDependencies(const ApplicationStartupDependencies &dependencies)
{
    m_dependencies = dependencies;
}

void ApplicationStartup::run()
{
    loadScripts();
    loadSessions();
}

void ApplicationStartup::loadScripts()
{
    m_dependencies.scriptController->loadScripts();
    m_dependencies.modelRefresher->refreshScripts();
}

void ApplicationStartup::loadSessions()
{
    QString errorMessage;
    if (!m_dependencies.sessionRepository->loadSessions(errorMessage)) {
        m_dependencies.controllerContexts->reportStorageError(
            errorMessage.isEmpty() ? QStringLiteral("Cannot load sessions.") : errorMessage);
    }

    m_dependencies.sessionController->setCurrentIndex(0);
    m_dependencies.controllerContexts->reloadCurrentSessionHistory();
    m_dependencies.controllerContexts->notifySessionCollectionViewsChanged();
}
