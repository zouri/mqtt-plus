#include "viewmodels/scriptsviewmodel.h"

#include "models/scriptlibrarymodel.h"

#include <QVariantMap>

ScriptsViewModel::ScriptsViewModel(QObject *parent)
    : ScriptsViewModel(Dependencies {}, parent)
{
}

ScriptsViewModel::ScriptsViewModel(const Dependencies &dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(dependencies)
    , m_editor(this)
{
    if (m_dependencies.bindScriptLibraryChanged) {
        m_dependencies.bindScriptLibraryChanged(this, [this]() {
            emit scriptLibraryChanged();
        });
    }
}

ScriptLibraryModel *ScriptsViewModel::scripts() const { return m_dependencies.scripts; }
ScriptEditorViewModel *ScriptsViewModel::editor() { return &m_editor; }

int ScriptsViewModel::matchingScriptCount(const QString &filterText) const
{
    auto *scriptModel = scripts();
    if (!scriptModel) {
        return 0;
    }

    int visibleRows = 0;
    for (int row = 0; row < scriptModel->count(); ++row) {
        const QVariantMap script = scriptModel->rowAt(row);
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
    auto *scriptModel = scripts();
    if (!scriptModel) {
        if (m_editor.name().isEmpty()) {
            m_editor.newScript();
        }
        return;
    }

    const QString currentId = m_editor.currentScriptId();
    if (!currentId.isEmpty() && scriptModel->indexOfId(currentId) >= 0) {
        return;
    }

    if (scriptModel->count() > 0 && (!currentId.isEmpty() || m_editor.name().isEmpty())) {
        m_editor.loadScript(scriptModel->rowAt(0));
        return;
    }

    if (scriptModel->count() == 0 && m_editor.name().isEmpty()) {
        m_editor.newScript();
    }
}

bool ScriptsViewModel::selectScriptAt(int index)
{
    auto *scriptModel = scripts();
    if (!scriptModel || index < 0 || index >= scriptModel->count()) {
        return false;
    }
    m_editor.loadScript(scriptModel->rowAt(index));
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
    const QString savedId = upsertScript(
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
    return matchingScriptCount(filterText);
}

QString ScriptsViewModel::upsertScript(const QString &id, const QString &name, const QString &description, const QString &code)
{
    return m_dependencies.upsertScript ? m_dependencies.upsertScript(id, name, description, code) : QString();
}
