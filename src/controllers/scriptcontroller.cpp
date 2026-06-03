#include "scriptcontroller.h"

#include "app/appfacadeutils.h"
#include "services/payload/payloadcodec.h"
#include "services/scripting/luarunner.h"
#include "services/storage/scriptstore.h"

#include <QUuid>

using namespace AppFacadeUtils;

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

QString ScriptController::upsertScript(const QString &id, const QString &name, const QString &code)
{
    const QString trimmedName = name.trimmed();
    const QString scriptName = trimmedName.isEmpty() ? QStringLiteral("Untitled Script") : trimmedName;
    const QString scriptCode = code.trimmed().isEmpty() ? ScriptStore::defaultLuaScript() : code;
    const QString scriptId = id.trimmed().isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id.trimmed();
    const QString updatedAt = timestampNow();
    const QVector<ScriptEntry> previousScripts = m_scripts;

    for (auto &script : m_scripts) {
        if (script.id == scriptId) {
            script.name = scriptName;
            script.code = scriptCode;
            script.updatedAt = updatedAt;
            if (script.fileName.isEmpty()) {
                script.fileName = ScriptStore::scriptFileNameForId(script.id);
            }
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

ScriptController::DeleteResult ScriptController::deleteScript(const QString &id)
{
    const QString scriptId = id.trimmed();
    if (scriptId.isEmpty()) {
        return {};
    }

    const QVector<ScriptEntry> previousScripts = m_scripts;
    DeleteResult result;
    for (int i = 0; i < m_scripts.size(); ++i) {
        if (m_scripts.at(i).id == scriptId) {
            result.fileName = m_scripts.at(i).fileName;
            m_scripts.removeAt(i);
            result.success = true;
            break;
        }
    }
    if (!result.success) {
        return {};
    }

    if (!saveScripts()) {
        m_scripts = previousScripts;
        return {};
    }
    return result;
}

QVariantMap ScriptController::testScript(const QString &code, const QString &topic, const QString &payload, int format) const
{
    QString encodeError;
    QByteArray payloadBytes;
    const PayloadFormat payloadFormat = PayloadCodec::formatFromInt(format);
    if (!PayloadCodec::encodeForPublish(payloadFormat, payload, payloadBytes, encodeError)) {
        payloadBytes = payload.toUtf8();
    }

    QString decodeError;
    const QString decoded = PayloadCodec::decodeForDisplay(payloadFormat, payloadBytes, decodeError);

    LuaScriptContext context;
    context.topic = topic.trimmed().isEmpty() ? QStringLiteral("test/topic") : topic.trimmed();
    context.payloadBytes = payloadBytes;
    context.decodedPayload = decoded;
    context.decodeError = decodeError;
    context.format = payloadFormat;
    context.timestamp = timestampNow();

    const LuaScriptResult result = LuaRunner::run(code, context);
    QVariantMap row;
    row.insert(QStringLiteral("success"), result.success);
    row.insert(QStringLiteral("output"), result.output);
    row.insert(QStringLiteral("error"), result.error);
    if (!encodeError.isEmpty()) {
        row.insert(QStringLiteral("inputError"), encodeError);
    }
    return row;
}

void ScriptController::removeScriptFile(const QString &fileName) const
{
    ScriptStore::removeScriptFile(fileName);
}

bool ScriptController::saveScripts()
{
    QString errorMessage;
    if (ScriptStore::saveScripts(m_scripts, m_scriptIndexWritable, errorMessage)) {
        return true;
    }
    emit storageError(errorMessage.isEmpty() ? QStringLiteral("Cannot save scripts.") : errorMessage);
    return false;
}
