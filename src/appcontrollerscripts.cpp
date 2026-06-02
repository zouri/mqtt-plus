#include "appcontroller.h"

#include "appcontrollerutils.h"
#include "scriptstore.h"

#include <QUuid>

using namespace AppControllerUtils;

QString AppController::upsertScript(const QString &id, const QString &name, const QString &code)
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
            emit scriptLibraryChanged();
            emit currentSessionChanged();
            emit subscriptionsChanged();
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
    emit scriptLibraryChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    return script.id;
}

bool AppController::deleteScript(const QString &id)
{
    const QString scriptId = id.trimmed();
    if (scriptId.isEmpty()) {
        return false;
    }

    const QVector<ScriptEntry> previousScripts = m_scripts;
    QVector<QString> previousSubscriptionScriptIds;
    for (const auto &session : m_sessions) {
        for (const auto &subscription : session.subscriptions) {
            previousSubscriptionScriptIds.append(subscription.scriptId);
        }
    }

    bool removed = false;
    QString removedFileName;
    for (int i = 0; i < m_scripts.size(); ++i) {
        if (m_scripts.at(i).id == scriptId) {
            removedFileName = m_scripts.at(i).fileName;
            m_scripts.removeAt(i);
            removed = true;
            break;
        }
    }
    if (!removed) {
        return false;
    }

    for (auto &session : m_sessions) {
        for (auto &subscription : session.subscriptions) {
            if (subscription.scriptId == scriptId) {
                subscription.scriptId.clear();
            }
        }
    }

    if (!saveScripts()) {
        m_scripts = previousScripts;
        int subscriptionIndex = 0;
        for (auto &session : m_sessions) {
            for (auto &subscription : session.subscriptions) {
                subscription.scriptId = previousSubscriptionScriptIds.value(subscriptionIndex);
                ++subscriptionIndex;
            }
        }
        return false;
    }
    ScriptStore::removeScriptFile(removedFileName);
    saveSessions();
    emit scriptLibraryChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit sessionsChanged();
    return true;
}

QVariantMap AppController::testScript(const QString &code, const QString &topic, const QString &payload, int format) const
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

const AppController::ScriptEntry *AppController::scriptById(const QString &id) const
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

QString AppController::scriptName(const QString &id) const
{
    const auto *script = scriptById(id);
    return script ? script->name : QString();
}

void AppController::loadScripts()
{
    const ScriptStore::LoadResult result = ScriptStore::loadScripts();
    m_scripts = result.scripts;
    m_scriptIndexWritable = result.indexWritable;
}

bool AppController::saveScripts()
{
    return ScriptStore::saveScripts(m_scripts, m_scriptIndexWritable);
}

