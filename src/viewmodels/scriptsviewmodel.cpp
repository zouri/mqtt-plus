#include "viewmodels/scriptsviewmodel.h"

#include "app/applicationcore.h"
#include "models/scriptlibrarymodel.h"

ScriptsViewModel::ScriptsViewModel(ApplicationCore *core, QObject *parent)
    : QObject(parent)
    , m_core(core)
    , m_editor(this)
{
    if (!m_core) {
        return;
    }
    connect(m_core, &ApplicationCore::scriptLibraryChanged, this, &ScriptsViewModel::scriptLibraryChanged);
    connect(m_core, &ApplicationCore::scriptTestSamplesChanged, this, &ScriptsViewModel::scriptTestSamplesChanged);
}

ScriptLibraryModel *ScriptsViewModel::scripts() const { return m_core ? m_core->scripts() : nullptr; }
ScriptTestSamplesModel *ScriptsViewModel::scriptTestSamples() const { return m_core ? m_core->scriptTestSamples() : nullptr; }
ScriptEditorViewModel *ScriptsViewModel::editor() { return &m_editor; }
QStringList ScriptsViewModel::payloadFormats() const { return m_core ? m_core->payloadFormats() : QStringList {}; }

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
    return m_core ? m_core->upsertScript(id, name, description, code) : QString();
}

bool ScriptsViewModel::deleteScript(const QString &id)
{
    return m_core && m_core->deleteScript(id);
}

QVariantMap ScriptsViewModel::testScript(const QString &code, const QString &topic, const QString &payload, int format) const
{
    return m_core ? m_core->testScript(code, topic, payload, format) : QVariantMap {};
}
