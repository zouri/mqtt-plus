#include "app/scriptlibraryfacade.h"

#include "controllers/scriptcontroller.h"
#include "controllers/sessioncontroller.h"

#include <utility>

ScriptLibraryFacade::ScriptLibraryFacade(Dependencies dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(std::move(dependencies))
{
}

ScriptLibraryModel *ScriptLibraryFacade::scripts()
{
    return m_dependencies.scriptsModel;
}

ScriptTestSamplesModel *ScriptLibraryFacade::scriptTestSamples()
{
    return m_dependencies.scriptTestSamplesModel;
}

QString ScriptLibraryFacade::upsertScript(
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
    if (m_dependencies.refreshScriptsModel) {
        m_dependencies.refreshScriptsModel();
    }
    if (m_dependencies.emitScriptLibraryChanged) {
        m_dependencies.emitScriptLibraryChanged();
    }
    if (m_dependencies.notifyCurrentSessionAndSubscriptionsChanged) {
        m_dependencies.notifyCurrentSessionAndSubscriptionsChanged();
    }
    return savedId;
}

bool ScriptLibraryFacade::deleteScript(const QString &id)
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
    if (m_dependencies.refreshScriptsModel) {
        m_dependencies.refreshScriptsModel();
    }
    if (m_dependencies.emitScriptLibraryChanged) {
        m_dependencies.emitScriptLibraryChanged();
    }
    if (m_dependencies.notifySessionAndSubscriptionViewsChanged) {
        m_dependencies.notifySessionAndSubscriptionViewsChanged();
    }
    return sessionsSaved;
}

QVariantMap ScriptLibraryFacade::testScript(
    const QString &code,
    const QString &topic,
    const QString &payload,
    int format) const
{
    return m_dependencies.scriptController
        ? m_dependencies.scriptController->testScript(code, topic, payload, format)
        : QVariantMap {};
}
