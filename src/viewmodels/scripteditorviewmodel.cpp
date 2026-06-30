#include "viewmodels/scripteditorviewmodel.h"

ScriptEditorViewModel::ScriptEditorViewModel(QObject *parent)
    : QObject(parent)
{
}

QString ScriptEditorViewModel::currentScriptId() const { return m_currentScriptId; }
QString ScriptEditorViewModel::name() const { return m_name; }
QString ScriptEditorViewModel::description() const { return m_description; }
QString ScriptEditorViewModel::code() const { return m_code; }
QString ScriptEditorViewModel::validationStatus() const { return m_validationStatus; }
bool ScriptEditorViewModel::validationOk() const { return m_validationOk; }

bool ScriptEditorViewModel::hasUnsavedChanges() const
{
    return m_name != m_savedName || m_description != m_savedDescription || m_code != m_savedCode;
}

bool ScriptEditorViewModel::canSave() const
{
    return hasUnsavedChanges() || m_currentScriptId.isEmpty();
}

void ScriptEditorViewModel::setCurrentScriptId(const QString &id)
{
    if (m_currentScriptId == id) {
        return;
    }
    m_currentScriptId = id;
    emit currentScriptIdChanged();
    emitEditorStateChanged();
}

void ScriptEditorViewModel::setName(const QString &name)
{
    if (m_name == name) {
        return;
    }
    m_name = name;
    emit nameChanged();
    emitEditorStateChanged();
}

void ScriptEditorViewModel::setDescription(const QString &description)
{
    if (m_description == description) {
        return;
    }
    m_description = description;
    emit descriptionChanged();
    emitEditorStateChanged();
}

void ScriptEditorViewModel::setCode(const QString &code)
{
    if (m_code == code) {
        return;
    }
    m_code = code;
    emit codeChanged();
    emitEditorStateChanged();
}

QString ScriptEditorViewModel::defaultCode() const
{
    return QStringLiteral("function parse(ctx)\n    return ctx.decoded\nend\n");
}

void ScriptEditorViewModel::loadScript(const QVariantMap &row)
{
    setCurrentScriptId(row.value(QStringLiteral("id")).toString());
    setName(row.value(QStringLiteral("name")).toString());
    setDescription(row.value(QStringLiteral("description")).toString());
    setCode(row.value(QStringLiteral("code"), defaultCode()).toString());
    m_savedName = m_name;
    m_savedDescription = m_description;
    m_savedCode = m_code;
    setValidationStatus(m_currentScriptId.isEmpty() ? QStringLiteral("Unsaved") : QStringLiteral("Saved"));
    setValidationOk(false);
    emitEditorStateChanged();
}

void ScriptEditorViewModel::newScript()
{
    setCurrentScriptId(QString());
    setName(QStringLiteral("New Lua Script"));
    setDescription(QStringLiteral("Decode MQTT payloads with Lua."));
    setCode(defaultCode());
    m_savedName = m_name;
    m_savedDescription = m_description;
    m_savedCode = m_code;
    setValidationStatus(QStringLiteral("Unsaved"));
    setValidationOk(false);
    emitEditorStateChanged();
}

bool ScriptEditorViewModel::validateStructure()
{
    const bool valid = m_code.contains(QStringLiteral("function parse"))
            && (m_code.trimmed().endsWith(QStringLiteral("end")) || m_code.contains(QStringLiteral("\nend")));
    setValidationOk(valid);
    setValidationStatus(valid
            ? QStringLiteral("Structure valid")
            : QStringLiteral("Structure invalid: define function parse(ctx) ... end"));
    return valid;
}

void ScriptEditorViewModel::markSaved(const QString &id)
{
    setCurrentScriptId(id);
    m_savedName = m_name;
    m_savedDescription = m_description;
    m_savedCode = m_code;
    setValidationStatus(QStringLiteral("Saved"));
    setValidationOk(true);
    emitEditorStateChanged();
}

void ScriptEditorViewModel::setValidationStatus(const QString &status)
{
    if (m_validationStatus == status) {
        return;
    }
    m_validationStatus = status;
    emit validationStatusChanged();
}

void ScriptEditorViewModel::setValidationOk(bool ok)
{
    if (m_validationOk == ok) {
        return;
    }
    m_validationOk = ok;
    emit validationOkChanged();
}

void ScriptEditorViewModel::emitEditorStateChanged()
{
    emit editorStateChanged();
}
