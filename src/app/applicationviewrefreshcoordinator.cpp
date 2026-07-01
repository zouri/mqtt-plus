#include "app/applicationviewrefreshcoordinator.h"

#include "app/applicationmodelrefresher.h"
#include "app/applicationcore.h"
#include "controllers/eventcontroller.h"
#include "controllers/sessioncontroller.h"
#include "models/eventstreammodel.h"

#include <QVariantList>

namespace {

SessionState *currentSession(const ApplicationViewRefreshDependencies &dependencies)
{
    return dependencies.sessionController->currentSession();
}

} // namespace

void ApplicationViewRefreshCoordinator::setDependencies(const ApplicationViewRefreshDependencies &dependencies)
{
    m_dependencies = dependencies;
}

void ApplicationViewRefreshCoordinator::refreshSessionsModel()
{
    m_dependencies.modelRefresher->refreshSessions();
}

void ApplicationViewRefreshCoordinator::refreshSubscriptionsModel()
{
    m_dependencies.modelRefresher->refreshSubscriptions(currentSession(m_dependencies));
}

void ApplicationViewRefreshCoordinator::refreshScriptsModel()
{
    m_dependencies.modelRefresher->refreshScripts();
}

void ApplicationViewRefreshCoordinator::refreshScriptTestSamplesModel()
{
    m_dependencies.modelRefresher->refreshScriptTestSamples(currentSession(m_dependencies));
}

void ApplicationViewRefreshCoordinator::reloadCurrentSessionHistory()
{
    m_dependencies.eventController->reloadCurrentSessionHistory();
}

void ApplicationViewRefreshCoordinator::notifyCurrentSessionViewsChanged()
{
    refreshSessionsModel();
    m_dependencies.core->notifyCurrentSessionChanged();
}

void ApplicationViewRefreshCoordinator::notifyCurrentSessionAndSubscriptionsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    m_dependencies.core->notifyCurrentSessionChanged();
    m_dependencies.core->notifySubscriptionsChanged();
}

void ApplicationViewRefreshCoordinator::notifySessionViewsChanged()
{
    refreshSessionsModel();
    m_dependencies.core->notifySessionsChanged();
    m_dependencies.core->notifyCurrentSessionChanged();
}

void ApplicationViewRefreshCoordinator::notifySessionAndSubscriptionViewsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    m_dependencies.core->notifySessionsChanged();
    m_dependencies.core->notifyCurrentSessionChanged();
    m_dependencies.core->notifySubscriptionsChanged();
}

void ApplicationViewRefreshCoordinator::notifySelectedSessionViewsChanged()
{
    refreshSubscriptionsModel();
    m_dependencies.messagesModel->setRows(currentSession(m_dependencies) ? currentSession(m_dependencies)->messageRows : QVariantList {});
    m_dependencies.logsModel->setRows(currentSession(m_dependencies) ? currentSession(m_dependencies)->logRows : QVariantList {});
    refreshScriptTestSamplesModel();
    m_dependencies.core->notifyCurrentSessionIndexChanged();
    m_dependencies.core->notifyCurrentSessionChanged();
    m_dependencies.core->notifySubscriptionsChanged();
    m_dependencies.core->notifyMessageStreamChanged();
    m_dependencies.core->notifyLogStreamChanged();
    m_dependencies.core->notifyScriptLibraryChanged();
}

void ApplicationViewRefreshCoordinator::notifySessionCollectionViewsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    m_dependencies.messagesModel->setRows(currentSession(m_dependencies) ? currentSession(m_dependencies)->messageRows : QVariantList {});
    m_dependencies.logsModel->setRows(currentSession(m_dependencies) ? currentSession(m_dependencies)->logRows : QVariantList {});
    refreshScriptsModel();
    refreshScriptTestSamplesModel();
    m_dependencies.core->notifySessionsChanged();
    m_dependencies.core->notifyCurrentSessionIndexChanged();
    m_dependencies.core->notifyCurrentSessionChanged();
    m_dependencies.core->notifySubscriptionsChanged();
    m_dependencies.core->notifyMessageStreamChanged();
    m_dependencies.core->notifyLogStreamChanged();
    m_dependencies.core->notifyScriptLibraryChanged();
}

void ApplicationViewRefreshCoordinator::notifyLanguageChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    m_dependencies.core->notifyCurrentSessionChanged();
    m_dependencies.core->notifySessionsChanged();
    m_dependencies.core->notifySubscriptionsChanged();
    m_dependencies.core->notifyLanguageChanged();
}

void ApplicationViewRefreshCoordinator::notifyHistoryPageSizeChanged()
{
    m_dependencies.eventController->reloadCurrentSessionHistory();
    m_dependencies.core->notifyMessageStreamChanged();
    m_dependencies.core->notifyLogStreamChanged();
    m_dependencies.core->notifyHistoryPageSizeChanged();
}

void ApplicationViewRefreshCoordinator::reportStorageError(const QString &message)
{
    if (message.isEmpty()) {
        return;
    }

    if (auto *session = currentSession(m_dependencies)) {
        session->lastError = message;
        m_dependencies.eventController->appendEvent(*session, QStringLiteral("Storage"), message);
    }

    notifySessionViewsChanged();
}

void ApplicationViewRefreshCoordinator::emitSessionsChanged()
{
    m_dependencies.core->notifySessionsChanged();
}

void ApplicationViewRefreshCoordinator::emitSubscriptionsChanged()
{
    m_dependencies.core->notifySubscriptionsChanged();
}

void ApplicationViewRefreshCoordinator::emitMessageStreamChanged()
{
    m_dependencies.core->notifyMessageStreamChanged();
}

void ApplicationViewRefreshCoordinator::emitLogStreamChanged()
{
    m_dependencies.core->notifyLogStreamChanged();
}

void ApplicationViewRefreshCoordinator::emitMessageStreamRowAppended(const QVariantMap &row)
{
    m_dependencies.core->notifyMessageStreamRowAppended(row);
}

void ApplicationViewRefreshCoordinator::emitLogStreamRowAppended(const QVariantMap &row)
{
    m_dependencies.core->notifyLogStreamRowAppended(row);
}
