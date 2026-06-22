#include "app/appfacade.h"

QString AppFacade::scriptName(const QString &id) const
{
    return m_scriptController.scriptName(id);
}

void AppFacade::loadScripts()
{
    m_scriptController.loadScripts();
    refreshScriptsModel();
}
