#include "app/appfacade.h"

QString AppFacade::upsertScript(
    const QString &id,
    const QString &name,
    const QString &description,
    const QString &code)
{
    const QString savedId = m_scriptController.upsertScript(id, name, description, code);
    if (savedId.isEmpty()) {
        return QString();
    }
    refreshScriptsModel();
    emit scriptLibraryChanged();
    notifyCurrentSessionAndSubscriptionsChanged();
    return savedId;
}

bool AppFacade::deleteScript(const QString &id)
{
    const QString scriptId = id.trimmed();
    if (scriptId.isEmpty()) {
        return false;
    }

    QVector<QString> previousSubscriptionScriptIds;
    for (const auto &session : m_sessionController.sessions()) {
        for (const auto &subscription : session.subscriptions) {
            previousSubscriptionScriptIds.append(subscription.scriptId);
        }
    }

    const ScriptController::DeleteResult result = m_scriptController.deleteScript(scriptId);
    if (!result.success) {
        return false;
    }

    for (auto &session : m_sessionController.sessions()) {
        for (auto &subscription : session.subscriptions) {
            if (subscription.scriptId == scriptId) {
                subscription.scriptId.clear();
            }
        }
    }

    m_scriptController.removeScriptFile(result.fileName);
    const bool sessionsSaved = saveSessions();
    if (!sessionsSaved) {
        int subscriptionIndex = 0;
        for (auto &session : m_sessionController.sessions()) {
            for (auto &subscription : session.subscriptions) {
                subscription.scriptId = previousSubscriptionScriptIds.value(subscriptionIndex);
                ++subscriptionIndex;
            }
        }
    }
    refreshScriptsModel();
    emit scriptLibraryChanged();
    notifySessionAndSubscriptionViewsChanged();
    return sessionsSaved;
}

QVariantMap AppFacade::testScript(const QString &code, const QString &topic, const QString &payload, int format) const
{
    return m_scriptController.testScript(code, topic, payload, format);
}

QString AppFacade::scriptName(const QString &id) const
{
    return m_scriptController.scriptName(id);
}

void AppFacade::loadScripts()
{
    m_scriptController.loadScripts();
    refreshScriptsModel();
}
