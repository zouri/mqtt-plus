#include "app/workbenchworkspace.h"

#include "services/apputils.h"
#include "controllers/eventcontroller.h"
#include "controllers/mqttcontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/subscriptioncontroller.h"
#include "domain/sessionconfig.h"
#include "models/eventstreammodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionfiltermodel.h"
#include "services/payload/payloadcodec.h"

#include <QCoreApplication>

using namespace AppUtils;

WorkbenchWorkspace::WorkbenchWorkspace(const WorkbenchWorkspaceDependencies &dependencies)
    : m_dependencies(dependencies)
{
}

void WorkbenchWorkspace::bindWorkbenchSignals(QObject *context, const WorkbenchCoreSignalHandlers &handlers)
{
    if (!context) {
        return;
    }

    if (handlers.currentSessionIndexChanged && m_dependencies.bindCurrentSessionIndexChanged) {
        m_dependencies.bindCurrentSessionIndexChanged(context, handlers.currentSessionIndexChanged);
    }
    if (handlers.currentSessionChanged && m_dependencies.bindCurrentSessionChanged) {
        m_dependencies.bindCurrentSessionChanged(context, handlers.currentSessionChanged);
    }
    if (handlers.messageStreamChanged && m_dependencies.bindMessageStreamChanged) {
        m_dependencies.bindMessageStreamChanged(context, handlers.messageStreamChanged);
    }
    if (handlers.messageStreamRowAppended && m_dependencies.bindMessageStreamRowAppended) {
        m_dependencies.bindMessageStreamRowAppended(context, [handler = handlers.messageStreamRowAppended](const QVariantMap &) {
            handler();
        });
    }
    if (handlers.scriptLibraryChanged && m_dependencies.bindScriptLibraryChanged) {
        m_dependencies.bindScriptLibraryChanged(context, handlers.scriptLibraryChanged);
    }
}

SessionListModel *WorkbenchWorkspace::sessions()
{
    return m_dependencies.sessions;
}

SubscriptionFilterModel *WorkbenchWorkspace::filteredSubscriptions()
{
    return m_dependencies.filteredSubscriptions;
}

EventStreamModel *WorkbenchWorkspace::messages()
{
    return m_dependencies.messages;
}

ScriptLibraryModel *WorkbenchWorkspace::scripts()
{
    return m_dependencies.scripts;
}

int WorkbenchWorkspace::currentSessionIndex() const
{
    return m_dependencies.sessionController ? m_dependencies.sessionController->currentIndex() : -1;
}

QVariantMap WorkbenchWorkspace::currentSession() const
{
    const auto *session = m_dependencies.sessionController ? m_dependencies.sessionController->currentSession() : nullptr;
    if (!session) {
        return {};
    }

    QVariantMap row;
    const auto *client = session->client;
    row.insert(QStringLiteral("id"), session->id);
    row.insert(QStringLiteral("name"), session->name);
    row.insert(QStringLiteral("host"), client ? client->hostname() : QString());
    row.insert(QStringLiteral("port"), client ? client->port() : SessionConfig::kDefaultPort);
    row.insert(QStringLiteral("transport"), session->transport);
    row.insert(QStringLiteral("transportLabel"), transportLabel(session->transport));
    row.insert(QStringLiteral("protocolVersion"), session->protocolVersion);
    row.insert(QStringLiteral("protocolVersionName"), protocolVersionLabel(session->protocolVersion));
    row.insert(QStringLiteral("clientId"), client ? client->clientId() : QString());
    row.insert(QStringLiteral("username"), client ? client->username() : QString());
    row.insert(QStringLiteral("cleanSession"), client ? client->cleanSession() : true);
    row.insert(QStringLiteral("keepAliveSeconds"), client ? client->keepAlive() : SessionConfig::kDefaultKeepAlive);
    row.insert(QStringLiteral("outputPaused"), session->outputPaused);
    row.insert(QStringLiteral("subscriptionCount"), session->subscriptions.size());
    return row;
}

QVariantMap WorkbenchWorkspace::sessionStatus() const
{
    const auto *session = m_dependencies.sessionController ? m_dependencies.sessionController->currentSession() : nullptr;
    if (!session) {
        return {};
    }

    const auto *client = session->client;
    const QString state = sessionStateName(*session, client);
    QString summary;
    if (state == QStringLiteral("connected")) {
        summary = QCoreApplication::translate("WorkbenchWorkspace", "%1 • %2:%3 • %4")
                      .arg(protocolVersionLabel(session->protocolVersion))
                      .arg(client ? client->hostname() : QString())
                      .arg(client ? client->port() : SessionConfig::kDefaultPort)
                      .arg(transportLabel(session->transport));
        if (session->sessionRestored) {
            summary.append(QCoreApplication::translate("WorkbenchWorkspace", " • session restored"));
        }
    } else if (state == QStringLiteral("connecting")) {
        summary = QCoreApplication::translate("WorkbenchWorkspace", "Connecting to %1:%2 over %3")
                      .arg(client ? client->hostname() : QString())
                      .arg(client ? client->port() : SessionConfig::kDefaultPort)
                      .arg(transportLabel(session->transport));
    } else if (state == QStringLiteral("disconnecting")) {
        summary = QCoreApplication::translate("WorkbenchWorkspace", "Disconnecting from broker");
    } else if (!session->lastError.isEmpty()) {
        summary = session->lastError;
    } else {
        summary = QCoreApplication::translate("WorkbenchWorkspace", "Disconnected");
    }

    QVariantMap row;
    row.insert(QStringLiteral("state"), state);
    row.insert(QStringLiteral("connected"), state == QStringLiteral("connected"));
    row.insert(QStringLiteral("summary"), summary);
    row.insert(QStringLiteral("lastError"), session->lastError);
    row.insert(QStringLiteral("hasError"), !session->lastError.isEmpty());
    row.insert(QStringLiteral("brokerInfo"), session->brokerInfo);
    row.insert(QStringLiteral("sessionRestored"), session->sessionRestored);
    row.insert(QStringLiteral("transportLabel"), transportLabel(session->transport));
    row.insert(QStringLiteral("protocolVersionName"), protocolVersionLabel(session->protocolVersion));
    return row;
}

QVariantMap WorkbenchWorkspace::publishStatus() const
{
    const auto *session = m_dependencies.sessionController ? m_dependencies.sessionController->currentSession() : nullptr;
    QVariantMap status = session ? session->publishStatus : defaultPublishStatus();
    status.insert(
        QStringLiteral("updatedAt"),
        displayTimestamp(status.value(QStringLiteral("updatedAt")).toString()));
    return status;
}

QStringList WorkbenchWorkspace::payloadFormats() const
{
    return PayloadCodec::formatNames();
}

void WorkbenchWorkspace::setCurrentSessionIndex(int index)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->setCurrentSessionIndex(index);
    }
}

QVariantMap WorkbenchWorkspace::defaultSessionConfig() const
{
    return m_dependencies.sessionController ? m_dependencies.sessionController->defaultSessionConfig() : QVariantMap {};
}

QVariantMap WorkbenchWorkspace::sessionConfigAt(int index) const
{
    return m_dependencies.sessionController ? m_dependencies.sessionController->sessionConfigAt(index) : QVariantMap {};
}

bool WorkbenchWorkspace::updateSessionConfigAt(int index, const QVariantMap &config)
{
    return m_dependencies.sessionController && m_dependencies.sessionController->updateSessionConfigAt(index, config);
}

void WorkbenchWorkspace::addSessionWithConfig(const QVariantMap &config)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->addSessionWithConfig(config);
    }
}

void WorkbenchWorkspace::duplicateSessionAt(int index)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->duplicateSessionAt(index);
    }
}

void WorkbenchWorkspace::removeSessionAt(int index)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->removeSessionAt(index);
    }
}

QString WorkbenchWorkspace::showSessionContextMenu(int index, const QPointF &globalPosition)
{
    const auto *sessionModel = sessions();
    if (!sessionModel || index < 0 || index >= sessionModel->count()) {
        return {};
    }

    return m_platformActions.showSessionContextMenu(sessionModel->count() > 1, globalPosition);
}

QString WorkbenchWorkspace::showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition)
{
    const QString normalizedTopic = topic.trimmed();
    const auto *subscriptionModel = filteredSubscriptions();
    if (!subscriptionModel || normalizedTopic.isEmpty()) {
        return {};
    }

    for (int row = 0; row < subscriptionModel->count(); ++row) {
        const QVariantMap subscription = subscriptionModel->rowAt(row);
        if (subscription.value(QStringLiteral("topic")).toString() == normalizedTopic) {
            return m_platformActions.showSubscriptionContextMenu(globalPosition);
        }
    }

    return {};
}

void WorkbenchWorkspace::connectCurrentSession()
{
    if (m_dependencies.mqttController) {
        m_dependencies.mqttController->connectCurrentSession();
    }
}

void WorkbenchWorkspace::disconnectCurrentSession()
{
    if (m_dependencies.mqttController) {
        m_dependencies.mqttController->disconnectCurrentSession();
    }
}

void WorkbenchWorkspace::setCurrentOutputPaused(bool paused)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->setCurrentOutputPaused(paused);
    }
}

bool WorkbenchWorkspace::upsertCurrentSubscription(
    const QString &topic,
    int qos,
    int format,
    const QString &scriptId,
    const QString &alias)
{
    return m_dependencies.subscriptionController
        && m_dependencies.subscriptionController->upsertCurrentSubscription(topic, qos, format, scriptId, alias);
}

bool WorkbenchWorkspace::updateCurrentSubscription(
    const QString &topic,
    const QString &newTopic,
    const QString &alias,
    const QString &scriptId)
{
    return m_dependencies.subscriptionController
        && m_dependencies.subscriptionController->updateCurrentSubscription(topic, newTopic, alias, scriptId);
}

void WorkbenchWorkspace::removeCurrentSubscription(const QString &topic)
{
    if (m_dependencies.subscriptionController) {
        m_dependencies.subscriptionController->removeCurrentSubscription(topic);
    }
}

void WorkbenchWorkspace::setCurrentSubscriptionPaused(const QString &topic, bool paused)
{
    if (m_dependencies.subscriptionController) {
        m_dependencies.subscriptionController->setCurrentSubscriptionPaused(topic, paused);
    }
}

void WorkbenchWorkspace::publishCurrentSession(
    const QString &topic,
    const QString &payload,
    int format,
    int qos,
    bool retain)
{
    if (m_dependencies.mqttController) {
        m_dependencies.mqttController->publishCurrentSession(topic, payload, format, qos, retain);
    }
}

void WorkbenchWorkspace::copyTextToClipboard(const QString &text) const
{
    m_platformActions.copyTextToClipboard(text);
}

void WorkbenchWorkspace::clearCurrentMessages()
{
    if (m_dependencies.eventController) {
        m_dependencies.eventController->clearCurrentMessages();
    }
}

int WorkbenchWorkspace::loadOlderCurrentSessionMessages()
{
    return m_dependencies.eventController ? m_dependencies.eventController->loadOlderCurrentSessionMessages() : 0;
}
