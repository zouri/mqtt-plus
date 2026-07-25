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
    , m_editor(this)
{
    m_scripts.setScripts(m_scriptService.scripts());
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
ScriptEditorViewModel *ScriptsViewModel::editor() { return &m_editor; }

bool ScriptsViewModel::scriptMatchesFilter(
    const QString &name,
    const QString &description,
    const QString &code,
    const QString &filterText)
{
    const QString needle = filterText.trimmed().toLower();
    if (needle.isEmpty()) {
        return true;
    }
    return QStringLiteral("%1 %2 %3").arg(name, description, code).toLower().contains(needle);
}

void ScriptsViewModel::ensureEditorSelection()
{
    const QString currentId = m_editor.currentScriptId();
    if (!currentId.isEmpty() && m_scripts.indexOfId(currentId) >= 0) {
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

int ScriptsViewModel::visibleScriptCount(const QString &filterText) const
{
    int visibleRows = 0;
    for (int row = 0; row < m_scripts.rowCount(); ++row) {
        const QVariantMap script = m_scripts.rowAt(row);
        if (scriptMatchesFilter(
                script.value(QStringLiteral("name")).toString(),
                script.value(QStringLiteral("description")).toString(),
                script.value(QStringLiteral("code")).toString(),
                filterText)) {
            ++visibleRows;
        }
    }
    return visibleRows;
}
