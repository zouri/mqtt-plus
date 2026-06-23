#include "app/scriptsbus.h"

#include "controllers/scriptcontroller.h"
#include "controllers/sessioncontroller.h"

#include <utility>

ScriptsBus::ScriptsBus(Dependencies dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(std::move(dependencies))
{
}

ScriptLibraryModel *ScriptsBus::scripts()
{
    return m_dependencies.scriptsModel;
}

ScriptTestSamplesModel *ScriptsBus::scriptTestSamples()
{
    return m_dependencies.scriptTestSamplesModel;
}

QString ScriptsBus::upsertScript(
    const QString &id,
    const QString &name,
    const QString &description,
    const QString &code)
{
    if (!m_dependencies.scriptController) {
        return QString();
    }
    const QString savedId = m_dependencies.scriptController->upsertScript(id, name, description, code);
    if (savedId.isEmpty()) {
        return QString();
    }
    if (m_dependencies.syncScriptsModel) {
        m_dependencies.syncScriptsModel();
    }
    if (m_dependencies.publishScriptsChanged) {
        m_dependencies.publishScriptsChanged();
    }
    if (m_dependencies.publishCurrentSessionAndSubscriptionsChanged) {
        m_dependencies.publishCurrentSessionAndSubscriptionsChanged();
    }
    return savedId;
}

bool ScriptsBus::deleteScript(const QString &id)
{
    const QString scriptId = id.trimmed();
    if (scriptId.isEmpty()) {
        return false;
    }
    if (!m_dependencies.scriptController || !m_dependencies.sessionController) {
        return false;
    }

    QVector<QString> previousSubscriptionScriptIds;
    for (const auto &session : m_dependencies.sessionController->sessions()) {
        for (const auto &subscription : session.subscriptions) {
            previousSubscriptionScriptIds.append(subscription.scriptId);
        }
    }

    const ScriptController::DeleteResult result = m_dependencies.scriptController->deleteScript(scriptId);
    if (!result.success) {
        return false;
    }

    for (auto &session : m_dependencies.sessionController->sessions()) {
        for (auto &subscription : session.subscriptions) {
            if (subscription.scriptId == scriptId) {
                subscription.scriptId.clear();
            }
        }
    }

    m_dependencies.scriptController->removeScriptFile(result.fileName);
    const bool sessionsSaved = m_dependencies.saveSessions ? m_dependencies.saveSessions() : false;
    if (!sessionsSaved) {
        int subscriptionIndex = 0;
        for (auto &session : m_dependencies.sessionController->sessions()) {
            for (auto &subscription : session.subscriptions) {
                subscription.scriptId = previousSubscriptionScriptIds.value(subscriptionIndex);
                ++subscriptionIndex;
            }
        }
    }
    if (m_dependencies.syncScriptsModel) {
        m_dependencies.syncScriptsModel();
    }
    if (m_dependencies.publishScriptsChanged) {
        m_dependencies.publishScriptsChanged();
    }
    if (m_dependencies.publishSessionAndSubscriptionViewsChanged) {
        m_dependencies.publishSessionAndSubscriptionViewsChanged();
    }
    return sessionsSaved;
}

QVariantMap ScriptsBus::testScript(
    const QString &code,
    const QString &topic,
    const QString &payload,
    int format) const
{
    return m_dependencies.scriptController
        ? m_dependencies.scriptController->testScript(code, topic, payload, format)
        : QVariantMap {};
}
