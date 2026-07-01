#include "app/scriptsworkspace.h"

#include "controllers/scriptcontroller.h"

ScriptsWorkspace::ScriptsWorkspace(const ScriptsWorkspaceDependencies &dependencies)
    : m_dependencies(dependencies)
{
}

void ScriptsWorkspace::bindScriptsSignals(QObject *context, const ScriptsCoreSignalHandlers &handlers)
{
    if (m_dependencies.bindScriptLibraryChanged && handlers.scriptLibraryChanged) {
        m_dependencies.bindScriptLibraryChanged(context, handlers.scriptLibraryChanged);
    }
}

ScriptLibraryModel *ScriptsWorkspace::scripts()
{
    return m_dependencies.scripts;
}

QString ScriptsWorkspace::upsertScript(
    const QString &id,
    const QString &name,
    const QString &description,
    const QString &code)
{
    if (!m_dependencies.scriptController) {
        return {};
    }

    const QString savedId = m_dependencies.scriptController->upsertScript(id, name, description, code);
    if (savedId.isEmpty()) {
        return {};
    }
    if (m_dependencies.refreshScriptsModel) {
        m_dependencies.refreshScriptsModel();
    }
    if (m_dependencies.emitScriptLibraryChanged) {
        m_dependencies.emitScriptLibraryChanged();
    }
    if (m_dependencies.notifyCurrentSessionAndSubscriptionsChanged) {
        m_dependencies.notifyCurrentSessionAndSubscriptionsChanged();
    }
    return savedId;
}
