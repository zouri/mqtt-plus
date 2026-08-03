#include "viewmodels/scriptsviewmodel.h"

#include "models/scriptlibrarymodel.h"
#include "usecases/scriptservice.h"

#include <QVariantMap>

ScriptsViewModel::ScriptsViewModel(
    ScriptService &scriptService,
    ScriptLibraryModel &scripts,
    QObject *parent)
    : QObject(parent)
    , m_scriptService(scriptService)
    , m_scripts(scripts)
    , m_filteredScripts(this)
    , m_editor(this)
{
    m_scripts.setScripts(m_scriptService.scripts());
    m_filteredScripts.setSourceModel(&m_scripts);
    connect(
        &m_scriptService,
        &ScriptService::scriptsChanged,
        this,
        [this]() {
            m_scripts.setScripts(m_scriptService.scripts());
            emit scriptLibraryChanged();
        });
}

ScriptLibraryModel *ScriptsViewModel::scripts() const { return &m_scripts; }
ScriptFilterModel *ScriptsViewModel::filteredScripts() { return &m_filteredScripts; }
ScriptEditorViewModel *ScriptsViewModel::editor() { return &m_editor; }

void ScriptsViewModel::ensureEditorSelection()
{
    const QString currentId = m_editor.currentScriptId();
    if (!currentId.isEmpty() && m_scripts.indexOfId(currentId) >= 0) {
        return;
    }
    if (m_editor.hasUnsavedChanges()) {
        return;
    }

    if (m_scripts.rowCount() > 0 && (!currentId.isEmpty() || m_editor.name().isEmpty())) {
        m_editor.loadScript(m_scripts.rowAt(0));
        return;
    }

    if (m_scripts.rowCount() == 0 && m_editor.name().isEmpty()) {
        m_editor.newScript();
    }
}

bool ScriptsViewModel::selectScriptAt(int index)
{
    if (index < 0 || index >= m_scripts.rowCount()) {
        return false;
    }
    m_editor.loadScript(m_scripts.rowAt(index));
    return true;
}

bool ScriptsViewModel::selectFilteredScriptAt(int index)
{
    if (index < 0 || index >= m_filteredScripts.rowCount()) {
        return false;
    }

    const QModelIndex sourceIndex = m_filteredScripts.mapToSource(m_filteredScripts.index(index, 0));
    return selectScriptAt(sourceIndex.row());
}

void ScriptsViewModel::setScriptFilterText(const QString &filterText)
{
    m_filteredScripts.setFilterText(filterText);
}

void ScriptsViewModel::newScript()
{
    m_editor.newScript();
}

bool ScriptsViewModel::validateEditorStructure()
{
    return m_editor.validateStructure();
}

bool ScriptsViewModel::saveEditor()
{
    const QString savedId = m_scriptService.upsertScript(
        m_editor.currentScriptId(),
        m_editor.name(),
        m_editor.description(),
        m_editor.code());
    if (savedId.isEmpty()) {
        return false;
    }
    m_editor.markSaved(savedId);
    return true;
}
