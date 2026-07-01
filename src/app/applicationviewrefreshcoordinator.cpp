#include "app/applicationviewrefreshcoordinator.h"

#include "app/applicationmodelrefresher.h"
#include "app/applicationnotifier.h"
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

void ApplicationViewRefreshCoordinator::notifyCurrentSessionViewsChanged()
{
    refreshSessionsModel();
    m_dependencies.notifier->notifyCurrentSessionChanged();
}

void ApplicationViewRefreshCoordinator::notifyCurrentSessionAndSubscriptionsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    m_dependencies.notifier->notifyCurrentSessionChanged();
    m_dependencies.notifier->notifySubscriptionsChanged();
}

void ApplicationViewRefreshCoordinator::notifySessionViewsChanged()
{
    refreshSessionsModel();
    m_dependencies.notifier->notifySessionsChanged();
    m_dependencies.notifier->notifyCurrentSessionChanged();
}

void ApplicationViewRefreshCoordinator::notifySessionAndSubscriptionViewsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    m_dependencies.notifier->notifySessionsChanged();
    m_dependencies.notifier->notifyCurrentSessionChanged();
    m_dependencies.notifier->notifySubscriptionsChanged();
}

void ApplicationViewRefreshCoordinator::notifySelectedSessionViewsChanged()
{
    refreshSubscriptionsModel();
    m_dependencies.messagesModel->setRows(currentSession(m_dependencies) ? currentSession(m_dependencies)->messageRows : QVariantList {});
    m_dependencies.logsModel->setRows(currentSession(m_dependencies) ? currentSession(m_dependencies)->logRows : QVariantList {});
    refreshScriptTestSamplesModel();
    m_dependencies.notifier->notifyCurrentSessionIndexChanged();
    m_dependencies.notifier->notifyCurrentSessionChanged();
    m_dependencies.notifier->notifySubscriptionsChanged();
    m_dependencies.notifier->notifyMessageStreamChanged();
    m_dependencies.notifier->notifyLogStreamChanged();
    m_dependencies.notifier->notifyScriptLibraryChanged();
}

void ApplicationViewRefreshCoordinator::notifySessionCollectionViewsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    m_dependencies.messagesModel->setRows(currentSession(m_dependencies) ? currentSession(m_dependencies)->messageRows : QVariantList {});
    m_dependencies.logsModel->setRows(currentSession(m_dependencies) ? currentSession(m_dependencies)->logRows : QVariantList {});
    refreshScriptsModel();
    refreshScriptTestSamplesModel();
    m_dependencies.notifier->notifySessionsChanged();
    m_dependencies.notifier->notifyCurrentSessionIndexChanged();
    m_dependencies.notifier->notifyCurrentSessionChanged();
    m_dependencies.notifier->notifySubscriptionsChanged();
    m_dependencies.notifier->notifyMessageStreamChanged();
    m_dependencies.notifier->notifyLogStreamChanged();
    m_dependencies.notifier->notifyScriptLibraryChanged();
}

void ApplicationViewRefreshCoordinator::notifyLanguageChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    m_dependencies.notifier->notifyCurrentSessionChanged();
    m_dependencies.notifier->notifySessionsChanged();
    m_dependencies.notifier->notifySubscriptionsChanged();
    m_dependencies.notifier->notifyLanguageChanged();
}

void ApplicationViewRefreshCoordinator::notifyHistoryPageSizeChanged()
{
    m_dependencies.eventController->reloadCurrentSessionHistory();
    m_dependencies.notifier->notifyMessageStreamChanged();
    m_dependencies.notifier->notifyLogStreamChanged();
    m_dependencies.notifier->notifyHistoryPageSizeChanged();
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
    m_dependencies.notifier->notifySessionsChanged();
}

void ApplicationViewRefreshCoordinator::emitSubscriptionsChanged()
{
    m_dependencies.notifier->notifySubscriptionsChanged();
}

void ApplicationViewRefreshCoordinator::emitMessageStreamChanged()
{
    m_dependencies.notifier->notifyMessageStreamChanged();
}

void ApplicationViewRefreshCoordinator::emitLogStreamChanged()
{
    m_dependencies.notifier->notifyLogStreamChanged();
}

void ApplicationViewRefreshCoordinator::emitMessageStreamRowAppended(const QVariantMap &row)
{
    m_dependencies.notifier->notifyMessageStreamRowAppended(row);
}

void ApplicationViewRefreshCoordinator::emitLogStreamRowAppended(const QVariantMap &row)
{
    m_dependencies.notifier->notifyLogStreamRowAppended(row);
}
