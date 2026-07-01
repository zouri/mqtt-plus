#include "app/applicationviewrefreshcoordinator.h"

#include "app/applicationcore.h"
#include "controllers/eventcontroller.h"
#include "controllers/scriptcontroller.h"
#include "controllers/sessioncontroller.h"
#include "models/eventstreammodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/scripttestsamplesmodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionlistmodel.h"

#include <QVariantList>

namespace {

SessionState *currentSession(const ApplicationViewRefreshDependencies &deps)
{
    if (!deps.sessionController) {
        return nullptr;
    }
    return deps.sessionController->currentSession();
}

} // namespace

void ApplicationViewRefreshCoordinator::setDependencies(const ApplicationViewRefreshDependencies &dependencies)
{
    m_deps = dependencies;
}

void ApplicationViewRefreshCoordinator::refreshSessionsModel()
{
    if (m_deps.sessionsModel) {
        m_deps.sessionsModel->notifyRefresh();
    }
}

void ApplicationViewRefreshCoordinator::refreshSubscriptionsModel()
{
    if (m_deps.subscriptionsModel) {
        m_deps.subscriptionsModel->setSource(currentSession(m_deps));
    }
}

void ApplicationViewRefreshCoordinator::refreshScriptsModel()
{
    if (m_deps.scriptsModel) {
        m_deps.scriptsModel->notifyRefresh();
    }
}

void ApplicationViewRefreshCoordinator::refreshScriptTestSamplesModel()
{
    if (m_deps.scriptTestSamplesModel) {
        auto *session = currentSession(m_deps);
        m_deps.scriptTestSamplesModel->setSource(session ? &session->messageRows : nullptr);
    }
}

void ApplicationViewRefreshCoordinator::reloadCurrentSessionHistory()
{
    if (m_deps.eventController) {
        m_deps.eventController->reloadCurrentSessionHistory();
    }
}

void ApplicationViewRefreshCoordinator::notifyCurrentSessionViewsChanged()
{
    refreshSessionsModel();
    if (m_deps.core) {
        m_deps.core->notifyCurrentSessionChanged();
    }
}

void ApplicationViewRefreshCoordinator::notifyCurrentSessionAndSubscriptionsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    if (m_deps.core) {
        m_deps.core->notifyCurrentSessionChanged();
        m_deps.core->notifySubscriptionsChanged();
    }
}

void ApplicationViewRefreshCoordinator::notifySessionViewsChanged()
{
    refreshSessionsModel();
    if (m_deps.core) {
        m_deps.core->notifySessionsChanged();
        m_deps.core->notifyCurrentSessionChanged();
    }
}

void ApplicationViewRefreshCoordinator::notifySessionAndSubscriptionViewsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    if (m_deps.core) {
        m_deps.core->notifySessionsChanged();
        m_deps.core->notifyCurrentSessionChanged();
        m_deps.core->notifySubscriptionsChanged();
    }
}

void ApplicationViewRefreshCoordinator::notifySelectedSessionViewsChanged()
{
    refreshSubscriptionsModel();
    if (m_deps.messagesModel) {
        auto *session = currentSession(m_deps);
        m_deps.messagesModel->setRows(session ? session->messageRows : QVariantList {});
    }
    if (m_deps.logsModel) {
        auto *session = currentSession(m_deps);
        m_deps.logsModel->setRows(session ? session->logRows : QVariantList {});
    }
    refreshScriptTestSamplesModel();
    if (m_deps.core) {
        m_deps.core->notifyCurrentSessionIndexChanged();
        m_deps.core->notifyCurrentSessionChanged();
        m_deps.core->notifySubscriptionsChanged();
        m_deps.core->notifyMessageStreamChanged();
        m_deps.core->notifyLogStreamChanged();
        m_deps.core->notifyScriptLibraryChanged();
    }
}

void ApplicationViewRefreshCoordinator::notifySessionCollectionViewsChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    if (m_deps.messagesModel) {
        auto *session = currentSession(m_deps);
        m_deps.messagesModel->setRows(session ? session->messageRows : QVariantList {});
    }
    if (m_deps.logsModel) {
        auto *session = currentSession(m_deps);
        m_deps.logsModel->setRows(session ? session->logRows : QVariantList {});
    }
    refreshScriptsModel();
    refreshScriptTestSamplesModel();
    if (m_deps.core) {
        m_deps.core->notifySessionsChanged();
        m_deps.core->notifyCurrentSessionIndexChanged();
        m_deps.core->notifyCurrentSessionChanged();
        m_deps.core->notifySubscriptionsChanged();
        m_deps.core->notifyMessageStreamChanged();
        m_deps.core->notifyLogStreamChanged();
        m_deps.core->notifyScriptLibraryChanged();
    }
}

void ApplicationViewRefreshCoordinator::notifyLanguageChanged()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    if (m_deps.core) {
        m_deps.core->notifyCurrentSessionChanged();
        m_deps.core->notifySessionsChanged();
        m_deps.core->notifySubscriptionsChanged();
        m_deps.core->notifyLanguageChanged();
    }
}

void ApplicationViewRefreshCoordinator::notifyHistoryPageSizeChanged()
{
    if (m_deps.eventController) {
        m_deps.eventController->reloadCurrentSessionHistory();
    }
    if (m_deps.core) {
        m_deps.core->notifyMessageStreamChanged();
        m_deps.core->notifyLogStreamChanged();
        m_deps.core->notifyHistoryPageSizeChanged();
    }
}

void ApplicationViewRefreshCoordinator::reportStorageError(const QString &message)
{
    if (message.isEmpty()) {
        return;
    }

    if (auto *session = currentSession(m_deps)) {
        session->lastError = message;
        if (m_deps.eventController) {
            m_deps.eventController->appendEvent(*session, QStringLiteral("Storage"), message);
        }
    }

    notifySessionViewsChanged();
}

void ApplicationViewRefreshCoordinator::emitSessionsChanged()
{
    if (m_deps.core) {
        m_deps.core->notifySessionsChanged();
    }
}

void ApplicationViewRefreshCoordinator::emitSubscriptionsChanged()
{
    if (m_deps.core) {
        m_deps.core->notifySubscriptionsChanged();
    }
}

void ApplicationViewRefreshCoordinator::emitMessageStreamChanged()
{
    if (m_deps.core) {
        m_deps.core->notifyMessageStreamChanged();
    }
}

void ApplicationViewRefreshCoordinator::emitLogStreamChanged()
{
    if (m_deps.core) {
        m_deps.core->notifyLogStreamChanged();
    }
}

void ApplicationViewRefreshCoordinator::emitMessageStreamRowAppended(const QVariantMap &row)
{
    if (m_deps.core) {
        m_deps.core->notifyMessageStreamRowAppended(row);
    }
}

void ApplicationViewRefreshCoordinator::emitLogStreamRowAppended(const QVariantMap &row)
{
    if (m_deps.core) {
        m_deps.core->notifyLogStreamRowAppended(row);
    }
}
