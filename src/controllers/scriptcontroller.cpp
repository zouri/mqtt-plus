#include "scriptcontroller.h"

#include "services/apputils.h"
#include "services/storage/scriptstore.h"

#include <QUuid>

using namespace AppUtils;

ScriptController::ScriptController(QObject *parent)
    : QObject(parent)
{
}

const QVector<ScriptEntry> &ScriptController::scripts() const
{
    return m_scripts;
}

const ScriptEntry *ScriptController::scriptById(const QString &id) const
{
    if (id.trimmed().isEmpty()) {
        return nullptr;
    }
    for (const auto &script : m_scripts) {
        if (script.id == id) {
            return &script;
        }
    }
    return nullptr;
}

QString ScriptController::scriptName(const QString &id) const
{
    const auto *script = scriptById(id);
    return script ? script->name : QString();
}

void ScriptController::loadScripts()
{
    const ScriptStore::LoadResult result = ScriptStore::loadScripts();
    m_scripts = result.scripts;
    m_scriptIndexWritable = result.indexWritable;
}

QString ScriptController::upsertScript(
    const QString &id,
    const QString &name,
    const QString &description,
    const QString &code)
{
    const QString trimmedName = name.trimmed();
    const QString scriptName = trimmedName.isEmpty() ? tr("Untitled Script") : trimmedName;
    const QString scriptDescription = description.trimmed();
    const QString scriptCode = code.trimmed().isEmpty() ? ScriptStore::defaultLuaScript() : code;
    const QString scriptId = id.trimmed().isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id.trimmed();
    const QString updatedAt = timestampNow();
    const QVector<ScriptEntry> previousScripts = m_scripts;

    for (auto &script : m_scripts) {
        if (script.id == scriptId) {
            script.name = scriptName;
            script.description = scriptDescription;
            script.code = scriptCode;
            script.updatedAt = updatedAt;
            if (!saveScripts()) {
                m_scripts = previousScripts;
                return QString();
            }
            return script.id;
        }
    }

    ScriptEntry script;
    script.id = scriptId;
    script.name = scriptName;
    script.description = scriptDescription;
    script.code = scriptCode;
    script.updatedAt = updatedAt;
    script.fileName = ScriptStore::scriptFileNameForId(script.id);
    m_scripts.append(script);

    if (!saveScripts()) {
        m_scripts = previousScripts;
        return QString();
    }
    return script.id;
}

bool ScriptController::saveScripts()
{
    QString errorMessage;
    if (ScriptStore::saveScripts(m_scripts, m_scriptIndexWritable, errorMessage)) {
        return true;
    }
    emit storageError(errorMessage.isEmpty() ? tr("Cannot save scripts.") : errorMessage);
    return false;
}
