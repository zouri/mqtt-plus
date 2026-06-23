#include "app/workbenchbus.h"

#include "app/appfacadeutils.h"
#include "controllers/eventcontroller.h"
#include "controllers/mqttcontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/subscriptioncontroller.h"
#include "domain/session.h"
#include "domain/sessionconfig.h"
#include "domain/subscription.h"
#include "services/payload/payloadcodec.h"

#include <QAction>
#include <QClipboard>
#include <QCursor>
#include <QDateTime>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QPoint>
#include <QPointF>

#include <utility>

using namespace AppFacadeUtils;

namespace {
QPoint menuPosition(const QPointF &globalPosition)
{
    if (globalPosition.isNull()) {
        return QCursor::pos();
    }
    return globalPosition.toPoint();
}
} // namespace

WorkbenchBus::WorkbenchBus(Dependencies dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(std::move(dependencies))
{
}

SessionListModel *WorkbenchBus::sessions()
{
    return m_dependencies.sessionsModel;
}

SubscriptionFilterModel *WorkbenchBus::filteredSubscriptions()
{
    return m_dependencies.filteredSubscriptionsModel;
}

int WorkbenchBus::currentSessionIndex() const
{
    return m_dependencies.sessionController ? m_dependencies.sessionController->currentIndex() : -1;
}

QVariantMap WorkbenchBus::currentSession() const
{
    const auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
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

QVariantMap WorkbenchBus::sessionStatus() const
{
    const auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session) {
        return {};
    }

    const auto *client = session->client;
    const QString state = sessionStateName(*session, client);
    QString summary;
    if (state == QStringLiteral("connected")) {
        summary = tr("%1 • %2:%3 • %4")
                      .arg(protocolVersionLabel(session->protocolVersion))
                      .arg(client ? client->hostname() : QString())
                      .arg(client ? client->port() : SessionConfig::kDefaultPort)
                      .arg(transportLabel(session->transport));
        if (session->sessionRestored) {
            summary.append(tr(" • session restored"));
        }
    } else if (state == QStringLiteral("connecting")) {
        summary = tr("Connecting to %1:%2 over %3")
                      .arg(client ? client->hostname() : QString())
                      .arg(client ? client->port() : SessionConfig::kDefaultPort)
                      .arg(transportLabel(session->transport));
    } else if (state == QStringLiteral("disconnecting")) {
        summary = tr("Disconnecting from broker");
    } else if (!session->lastError.isEmpty()) {
        summary = session->lastError;
    } else {
        summary = tr("Disconnected");
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

QVariantMap WorkbenchBus::publishStatus() const
{
    const auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    QVariantMap status = session ? session->publishStatus : defaultPublishStatus();
    status.insert(
        QStringLiteral("updatedAt"),
        displayTimestamp(status.value(QStringLiteral("updatedAt")).toString()));
    return status;
}

QStringList WorkbenchBus::payloadFormats() const
{
    return PayloadCodec::formatNames();
}

EventStreamModel *WorkbenchBus::messages()
{
    return m_dependencies.messagesModel;
}

void WorkbenchBus::setCurrentSessionIndex(int index)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->setCurrentSessionIndex(index);
    }
}

QVariantMap WorkbenchBus::defaultSessionConfig() const
{
    return m_dependencies.sessionController ? m_dependencies.sessionController->defaultSessionConfig() : QVariantMap {};
}

QVariantMap WorkbenchBus::sessionConfigAt(int index) const
{
    return m_dependencies.sessionController ? m_dependencies.sessionController->sessionConfigAt(index) : QVariantMap {};
}

bool WorkbenchBus::updateSessionConfigAt(int index, const QVariantMap &config)
{
    return m_dependencies.sessionController
        && m_dependencies.sessionController->updateSessionConfigAt(index, config);
}

void WorkbenchBus::addSessionWithConfig(const QVariantMap &config)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->addSessionWithConfig(config);
    }
}

void WorkbenchBus::duplicateSessionAt(int index)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->duplicateSessionAt(index);
    }
}

void WorkbenchBus::removeSessionAt(int index)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->removeSessionAt(index);
    }
}

QString WorkbenchBus::showSessionContextMenu(int index, const QPointF &globalPosition)
{
    if (!m_dependencies.sessionController || !m_dependencies.sessionController->isValidIndex(index)) {
        return {};
    }

    QMenu menu;
    QAction *editAction = menu.addAction(tr("Edit"));
    QAction *copyAction = menu.addAction(tr("Copy"));
    QAction *deleteAction = menu.addAction(tr("Delete"));
    editAction->setIcon(QIcon(QStringLiteral(":/qt/qml/MqttPlusApp/resources/edit.svg")));
    deleteAction->setIcon(QIcon(QStringLiteral(":/qt/qml/MqttPlusApp/resources/delete.svg")));
    deleteAction->setEnabled(m_dependencies.sessionController->sessions().size() > 1);

    QAction *selectedAction = menu.exec(menuPosition(globalPosition));
    if (selectedAction == editAction) {
        return QStringLiteral("edit");
    }
    if (selectedAction == copyAction) {
        return QStringLiteral("copy");
    }
    if (selectedAction == deleteAction) {
        return QStringLiteral("delete");
    }
    return {};
}

QString WorkbenchBus::showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition)
{
    const SessionState *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session) {
        return {};
    }
    const auto *subscription = m_dependencies.subscriptionByTopic
        ? m_dependencies.subscriptionByTopic(session, topic.trimmed())
        : nullptr;
    if (!session || !subscription) {
        return {};
    }

    QMenu menu;
    QAction *editAction = menu.addAction(tr("Edit"));
    QAction *deleteAction = menu.addAction(tr("Delete"));

    QAction *selectedAction = menu.exec(menuPosition(globalPosition));
    if (selectedAction == editAction) {
        return QStringLiteral("edit");
    }
    if (selectedAction == deleteAction) {
        return QStringLiteral("delete");
    }
    return {};
}

void WorkbenchBus::connectCurrentSession()
{
    if (m_dependencies.mqttController) {
        m_dependencies.mqttController->connectCurrentSession();
    }
}

void WorkbenchBus::disconnectCurrentSession()
{
    if (m_dependencies.mqttController) {
        m_dependencies.mqttController->disconnectCurrentSession();
    }
}

void WorkbenchBus::setCurrentOutputPaused(bool paused)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->setCurrentOutputPaused(paused);
    }
}

bool WorkbenchBus::upsertCurrentSubscription(
    const QString &topic,
    int qos,
    int format,
    const QString &scriptId,
    const QString &alias)
{
    return m_dependencies.subscriptionController
        && m_dependencies.subscriptionController->upsertCurrentSubscription(topic, qos, format, scriptId, alias);
}

bool WorkbenchBus::updateCurrentSubscription(
    const QString &topic,
    const QString &newTopic,
    const QString &alias,
    const QString &scriptId)
{
    return m_dependencies.subscriptionController
        && m_dependencies.subscriptionController->updateCurrentSubscription(topic, newTopic, alias, scriptId);
}

void WorkbenchBus::removeCurrentSubscription(const QString &topic)
{
    if (m_dependencies.subscriptionController) {
        m_dependencies.subscriptionController->removeCurrentSubscription(topic);
    }
}

void WorkbenchBus::setCurrentSubscriptionPaused(const QString &topic, bool paused)
{
    if (m_dependencies.subscriptionController) {
        m_dependencies.subscriptionController->setCurrentSubscriptionPaused(topic, paused);
    }
}

void WorkbenchBus::publishCurrentSession(
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

void WorkbenchBus::copyTextToClipboard(const QString &text) const
{
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}

void WorkbenchBus::clearCurrentMessages()
{
    if (m_dependencies.eventController) {
        m_dependencies.eventController->clearCurrentMessages();
    }
}

int WorkbenchBus::loadOlderCurrentSessionMessages()
{
    return m_dependencies.eventController
        ? m_dependencies.eventController->loadOlderCurrentSessionMessages()
        : 0;
}
