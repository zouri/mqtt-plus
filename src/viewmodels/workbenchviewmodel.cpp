#include "viewmodels/workbenchviewmodel.h"

#include "controllers/eventhistoryservice.h"
#include "controllers/mqttsessionservice.h"
#include "controllers/sessionservice.h"
#include "controllers/subscriptionservice.h"
#include "domain/sessionconfig.h"
#include "services/apputils.h"
#include "services/payload/payloadcodec.h"

#include <QCoreApplication>

using namespace AppUtils;

namespace {

PublishDraftViewModel::Dependencies publishDraftDependencies(const WorkbenchViewModel::Dependencies &dependencies)
{
    return {
        dependencies.bindCurrentSessionChanged,
        [dependencies]() {
            const auto *session = dependencies.sessionController ? dependencies.sessionController->currentSession() : nullptr;
            if (!session) {
                return false;
            }
            return sessionStateName(*session, session->client) == QStringLiteral("connected");
        },
        [dependencies](const QString &topic, const QString &payload, int format, int qos, bool retain) {
            if (dependencies.mqttController) {
                dependencies.mqttController->publishCurrentSession(topic, payload, format, qos, retain);
            }
        },
    };
}

} // namespace

WorkbenchViewModel::WorkbenchViewModel(QObject *parent)
    : WorkbenchViewModel(Dependencies {}, parent)
{
}

WorkbenchViewModel::WorkbenchViewModel(const Dependencies &dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(dependencies)
    , m_publisher(publishDraftDependencies(dependencies), this)
{
    if (m_dependencies.bindCurrentSessionIndexChanged) {
        m_dependencies.bindCurrentSessionIndexChanged(this, [this]() {
            emit currentSessionIndexChanged();
        });
    }
    if (m_dependencies.bindCurrentSessionChanged) {
        m_dependencies.bindCurrentSessionChanged(this, [this]() {
            emit currentSessionChanged();
        });
    }
    if (m_dependencies.bindMessageStreamChanged) {
        m_dependencies.bindMessageStreamChanged(this, [this]() {
            emit messageStreamChanged();
        });
    }
    if (m_dependencies.bindMessageStreamRowAppended) {
        m_dependencies.bindMessageStreamRowAppended(this, [this](const QVariantMap &) {
            emit messageStreamRowAppended();
        });
    }
    if (m_dependencies.bindScriptLibraryChanged) {
        m_dependencies.bindScriptLibraryChanged(this, [this]() {
            refreshSubscriptionEditorScriptOptions();
        });
    }
    refreshSubscriptionEditorScriptOptions();
}

SessionListModel *WorkbenchViewModel::sessions() const { return m_dependencies.sessions; }
SubscriptionFilterModel *WorkbenchViewModel::filteredSubscriptions() const { return m_dependencies.filteredSubscriptions; }
EventStreamModel *WorkbenchViewModel::messages() const { return m_dependencies.messages; }
PublishDraftViewModel *WorkbenchViewModel::publisher() { return &m_publisher; }
SessionEditorViewModel *WorkbenchViewModel::sessionEditor() { return &m_sessionEditor; }
SubscriptionEditorViewModel *WorkbenchViewModel::subscriptionEditor() { return &m_subscriptionEditor; }
int WorkbenchViewModel::currentSessionIndex() const
{
    return m_dependencies.sessionController ? m_dependencies.sessionController->currentIndex() : -1;
}

QVariantMap WorkbenchViewModel::currentSession() const
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

QVariantMap WorkbenchViewModel::sessionStatus() const
{
    const auto *session = m_dependencies.sessionController ? m_dependencies.sessionController->currentSession() : nullptr;
    if (!session) {
        return {};
    }

    const auto *client = session->client;
    const QString state = sessionStateName(*session, client);
    QString summary;
    if (state == QStringLiteral("connected")) {
        summary = QCoreApplication::translate("WorkbenchViewModel", "%1 • %2:%3 • %4")
                      .arg(protocolVersionLabel(session->protocolVersion))
                      .arg(client ? client->hostname() : QString())
                      .arg(client ? client->port() : SessionConfig::kDefaultPort)
                      .arg(transportLabel(session->transport));
        if (session->sessionRestored) {
            summary.append(QCoreApplication::translate("WorkbenchViewModel", " • session restored"));
        }
    } else if (state == QStringLiteral("connecting")) {
        summary = QCoreApplication::translate("WorkbenchViewModel", "Connecting to %1:%2 over %3")
                      .arg(client ? client->hostname() : QString())
                      .arg(client ? client->port() : SessionConfig::kDefaultPort)
                      .arg(transportLabel(session->transport));
    } else if (state == QStringLiteral("disconnecting")) {
        summary = QCoreApplication::translate("WorkbenchViewModel", "Disconnecting from broker");
    } else if (!session->lastError.isEmpty()) {
        summary = session->lastError;
    } else {
        summary = QCoreApplication::translate("WorkbenchViewModel", "Disconnected");
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

QVariantMap WorkbenchViewModel::publishStatus() const
{
    const auto *session = m_dependencies.sessionController ? m_dependencies.sessionController->currentSession() : nullptr;
    QVariantMap status = session ? session->publishStatus : defaultPublishStatus();
    status.insert(
        QStringLiteral("updatedAt"),
        displayTimestamp(status.value(QStringLiteral("updatedAt")).toString()));
    return status;
}

QStringList WorkbenchViewModel::payloadFormats() const { return PayloadCodec::formatNames(); }
QString WorkbenchViewModel::pendingSubscriptionDeleteTopic() const { return m_pendingSubscriptionDeleteTopic; }
QString WorkbenchViewModel::pendingSubscriptionDeleteDisplayName() const { return m_pendingSubscriptionDeleteDisplayName; }

void WorkbenchViewModel::setCurrentSessionIndex(int index)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->setCurrentSessionIndex(index);
    }
}

void WorkbenchViewModel::openSessionEditorForCreate()
{
    m_sessionEditor.openForCreate(
        m_dependencies.sessionController
            ? m_dependencies.sessionController->defaultSessionConfig()
            : SessionEditorViewModel::defaultConfig(1));
}

void WorkbenchViewModel::openSessionEditorForEdit(int index)
{
    if (!m_dependencies.sessionController || !sessions() || index < 0 || index >= sessions()->count()) {
        return;
    }
    m_sessionEditor.openForEdit(index, m_dependencies.sessionController->sessionConfigAt(index));
}

bool WorkbenchViewModel::submitSessionEditor()
{
    if (!m_sessionEditor.validate()) {
        return false;
    }
    if (!m_dependencies.sessionController) {
        return false;
    }

    const QVariantMap config = m_sessionEditor.collectedConfig();
    if (!m_sessionEditor.editMode()) {
        m_dependencies.sessionController->addSessionWithConfig(config);
        return true;
    }

    const int index = m_sessionEditor.targetIndex();
    return index >= 0 && m_dependencies.sessionController->updateSessionConfigAt(index, config);
}

void WorkbenchViewModel::requestSessionDuplicate(int index)
{
    if (!m_dependencies.sessionController || !sessions() || index < 0 || index >= sessions()->count()) {
        return;
    }

    m_dependencies.sessionController->duplicateSessionAt(index);
}

void WorkbenchViewModel::requestSessionDelete(int index)
{
    if (!m_dependencies.sessionController || !sessions() || index < 0 || index >= sessions()->count() || sessions()->count() <= 1) {
        return;
    }

    m_dependencies.sessionController->removeSessionAt(index);
}

void WorkbenchViewModel::toggleCurrentSessionConnection()
{
    if (!m_dependencies.mqttController) {
        return;
    }

    const QString state = sessionStatus().value(QStringLiteral("state")).toString();
    if (state == QStringLiteral("connected")
        || state == QStringLiteral("connecting")
        || state == QStringLiteral("disconnecting")) {
        m_dependencies.mqttController->disconnectCurrentSession();
        return;
    }

    m_dependencies.mqttController->connectCurrentSession();
}

void WorkbenchViewModel::toggleCurrentOutputPaused(bool currentlyPaused)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->setCurrentOutputPaused(!currentlyPaused);
    }
}

void WorkbenchViewModel::refreshSubscriptionEditorScriptOptions()
{
    QVariantList options;
    if (auto *scriptModel = scriptLibrary()) {
        for (int row = 0; row < scriptModel->rowCount(); ++row) {
            options.append(scriptModel->rowAt(row));
        }
    }
    m_subscriptionEditor.setScriptOptions(options);
}

void WorkbenchViewModel::openSubscriptionEditorForCreate()
{
    refreshSubscriptionEditorScriptOptions();
    m_subscriptionEditor.openForCreate();
}

bool WorkbenchViewModel::openSubscriptionEditorForEdit(int filteredIndex)
{
    if (!filteredSubscriptions() || filteredIndex < 0 || filteredIndex >= filteredSubscriptions()->count()) {
        return false;
    }

    refreshSubscriptionEditorScriptOptions();
    m_subscriptionEditor.openForEdit(filteredSubscriptions()->rowAt(filteredIndex));
    return true;
}

bool WorkbenchViewModel::submitSubscriptionEditor()
{
    if (!m_dependencies.subscriptionController || !m_subscriptionEditor.canSubmit()) {
        return false;
    }

    const QVariantMap submission = m_subscriptionEditor.submission();
    if (submission.value(QStringLiteral("editMode")).toBool()) {
        return m_dependencies.subscriptionController->updateCurrentSubscription(
            submission.value(QStringLiteral("editTopic")).toString(),
            submission.value(QStringLiteral("topic")).toString(),
            submission.value(QStringLiteral("alias")).toString(),
            submission.value(QStringLiteral("scriptId")).toString());
    }

    return m_dependencies.subscriptionController->upsertCurrentSubscription(
        submission.value(QStringLiteral("topic")).toString(),
        submission.value(QStringLiteral("qos")).toInt(),
        submission.value(QStringLiteral("format")).toInt(),
        submission.value(QStringLiteral("scriptId")).toString(),
        submission.value(QStringLiteral("alias")).toString());
}
void WorkbenchViewModel::toggleCurrentSubscriptionPaused(const QString &topic, bool currentlyPaused)
{
    if (m_dependencies.subscriptionController) {
        m_dependencies.subscriptionController->setCurrentSubscriptionPaused(topic, !currentlyPaused);
    }
}

void WorkbenchViewModel::requestSubscriptionDelete(const QString &topic, const QString &displayName)
{
    if (m_pendingSubscriptionDeleteTopic == topic && m_pendingSubscriptionDeleteDisplayName == displayName) {
        emit subscriptionDeleteRequested(topic, displayName);
        return;
    }

    m_pendingSubscriptionDeleteTopic = topic;
    m_pendingSubscriptionDeleteDisplayName = displayName;
    emit pendingSubscriptionDeleteChanged();
    emit subscriptionDeleteRequested(topic, displayName);
}

void WorkbenchViewModel::cancelPendingSubscriptionDelete()
{
    clearPendingSubscriptionDelete();
}

bool WorkbenchViewModel::confirmPendingSubscriptionDelete()
{
    const QString topic = m_pendingSubscriptionDeleteTopic;
    if (topic.isEmpty() || !m_dependencies.subscriptionController) {
        clearPendingSubscriptionDelete();
        return false;
    }

    m_dependencies.subscriptionController->removeCurrentSubscription(topic);
    clearPendingSubscriptionDelete();
    return true;
}

void WorkbenchViewModel::copyMessageTopic(const QString &topic) const
{
    m_platformActions.copyTextToClipboard(topic);
}

void WorkbenchViewModel::copyMessagePayload(const QString &payload, const QString &testPayload) const
{
    m_platformActions.copyTextToClipboard(testPayload.isEmpty() ? payload : testPayload);
}

void WorkbenchViewModel::clearMessages()
{
    if (m_dependencies.eventController) {
        m_dependencies.eventController->clearCurrentMessages();
    }
}

int WorkbenchViewModel::loadOlderMessages()
{
    return m_dependencies.eventController ? m_dependencies.eventController->loadOlderCurrentSessionMessages() : 0;
}

ScriptLibraryModel *WorkbenchViewModel::scriptLibrary() const
{
    return m_dependencies.scripts;
}

void WorkbenchViewModel::clearPendingSubscriptionDelete()
{
    if (m_pendingSubscriptionDeleteTopic.isEmpty() && m_pendingSubscriptionDeleteDisplayName.isEmpty()) {
        return;
    }

    m_pendingSubscriptionDeleteTopic.clear();
    m_pendingSubscriptionDeleteDisplayName.clear();
    emit pendingSubscriptionDeleteChanged();
}
