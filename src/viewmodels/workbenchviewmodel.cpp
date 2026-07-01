#include "viewmodels/workbenchviewmodel.h"

#include "controllers/eventcontroller.h"
#include "controllers/mqttcontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/subscriptioncontroller.h"
#include "domain/sessionconfig.h"
#include "services/apputils.h"
#include "services/payload/payloadcodec.h"

#include <QCoreApplication>

using namespace AppUtils;

WorkbenchViewModel::WorkbenchViewModel(QObject *parent)
    : WorkbenchViewModel(Dependencies {}, parent)
{
}

WorkbenchViewModel::WorkbenchViewModel(const Dependencies &dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(dependencies)
{
    if (m_dependencies.bindCurrentSessionIndexChanged) {
        m_dependencies.bindCurrentSessionIndexChanged(this, [this]() {
            emit currentSessionIndexChanged();
        });
    }
    if (m_dependencies.bindCurrentSessionChanged) {
        m_dependencies.bindCurrentSessionChanged(this, [this]() {
            emit currentSessionChanged();
            emit canPublishChanged();
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
    syncSubscriptionFilterModel();
    refreshSubscriptionEditorScriptOptions();
}

SessionListModel *WorkbenchViewModel::sessions() const { return m_dependencies.sessions; }
SubscriptionFilterModel *WorkbenchViewModel::filteredSubscriptions() const { return m_dependencies.filteredSubscriptions; }
EventStreamModel *WorkbenchViewModel::messages() const { return m_dependencies.messages; }
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
QString WorkbenchViewModel::publishTopic() const { return m_publishTopic; }
QString WorkbenchViewModel::publishPayload() const { return m_publishPayload; }
int WorkbenchViewModel::publishFormat() const { return m_publishFormat; }
int WorkbenchViewModel::publishQos() const { return m_publishQos; }
bool WorkbenchViewModel::publishRetain() const { return m_publishRetain; }
bool WorkbenchViewModel::canPublish() const
{
    return sessionStatus().value(QStringLiteral("state")).toString() == QStringLiteral("connected")
        && !m_publishTopic.trimmed().isEmpty();
}
QString WorkbenchViewModel::subscriptionFilterText() const { return m_subscriptionFilterText; }
QString WorkbenchViewModel::subscriptionFilterMode() const { return m_subscriptionFilterMode; }
int WorkbenchViewModel::subscriptionFilterModeIndex() const { return subscriptionFilterModeIndexForMode(m_subscriptionFilterMode); }
bool WorkbenchViewModel::hasSubscriptionFilter() const
{
    return !m_subscriptionFilterText.trimmed().isEmpty() || m_subscriptionFilterMode != QStringLiteral("all");
}
QString WorkbenchViewModel::pendingSubscriptionDeleteTopic() const { return m_pendingSubscriptionDeleteTopic; }
QString WorkbenchViewModel::pendingSubscriptionDeleteDisplayName() const { return m_pendingSubscriptionDeleteDisplayName; }

void WorkbenchViewModel::setCurrentSessionIndex(int index)
{
    if (m_dependencies.sessionController) {
        m_dependencies.sessionController->setCurrentSessionIndex(index);
    }
}

void WorkbenchViewModel::setPublishTopic(const QString &topic)
{
    if (m_publishTopic == topic) {
        return;
    }

    const bool wasPublishable = canPublish();
    m_publishTopic = topic;
    emit publishTopicChanged();
    if (wasPublishable != canPublish()) {
        emit canPublishChanged();
    }
}

void WorkbenchViewModel::setPublishPayload(const QString &payload)
{
    if (m_publishPayload == payload) {
        return;
    }

    m_publishPayload = payload;
    emit publishPayloadChanged();
}

void WorkbenchViewModel::setPublishFormat(int format)
{
    if (m_publishFormat == format) {
        return;
    }

    m_publishFormat = format;
    emit publishFormatChanged();
}

void WorkbenchViewModel::setPublishQos(int qos)
{
    if (m_publishQos == qos) {
        return;
    }

    m_publishQos = qos;
    emit publishQosChanged();
}

void WorkbenchViewModel::setPublishRetain(bool retain)
{
    if (m_publishRetain == retain) {
        return;
    }

    m_publishRetain = retain;
    emit publishRetainChanged();
}

void WorkbenchViewModel::setSubscriptionFilterText(const QString &filterText)
{
    const QString oldText = m_subscriptionFilterText;
    const QString oldMode = m_subscriptionFilterMode;
    const QString trimmedText = filterText.trimmed();
    if (m_subscriptionFilterText == trimmedText) {
        return;
    }

    m_subscriptionFilterText = trimmedText;
    syncSubscriptionFilterModel();
    emitSubscriptionFilterSignals(oldText, oldMode);
}

void WorkbenchViewModel::setSubscriptionFilterMode(const QString &filterMode)
{
    const QString oldText = m_subscriptionFilterText;
    const QString oldMode = m_subscriptionFilterMode;
    const QString normalizedMode = normalizedSubscriptionFilterMode(filterMode);
    if (m_subscriptionFilterMode == normalizedMode) {
        return;
    }

    m_subscriptionFilterMode = normalizedMode;
    syncSubscriptionFilterModel();
    emitSubscriptionFilterSignals(oldText, oldMode);
}

void WorkbenchViewModel::setSubscriptionFilterModeIndex(int index)
{
    static const QStringList modes {
        QStringLiteral("all"),
        QStringLiteral("subscribed"),
        QStringLiteral("paused"),
    };
    setSubscriptionFilterMode(index >= 0 && index < modes.size() ? modes.at(index) : QStringLiteral("all"));
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

void WorkbenchViewModel::handleSessionContextMenu(int index, const QPointF &globalPosition)
{
    if (!m_dependencies.sessionController || !sessions() || index < 0 || index >= sessions()->count()) {
        return;
    }

    const QString action = m_platformActions.showSessionContextMenu(sessions()->count() > 1, globalPosition);
    if (action == QStringLiteral("edit")) {
        emit sessionEditRequested(index);
    } else if (action == QStringLiteral("copy")) {
        m_dependencies.sessionController->duplicateSessionAt(index);
    } else if (action == QStringLiteral("delete")) {
        m_dependencies.sessionController->removeSessionAt(index);
    }
}

void WorkbenchViewModel::handleSubscriptionContextMenu(int filteredIndex, const QString &topic, const QPointF &globalPosition)
{
    const QString normalizedTopic = topic.trimmed();
    if (!filteredSubscriptions() || normalizedTopic.isEmpty()) {
        return;
    }

    bool hasTopic = false;
    for (int row = 0; row < filteredSubscriptions()->count(); ++row) {
        const QVariantMap subscription = filteredSubscriptions()->rowAt(row);
        if (subscription.value(QStringLiteral("topic")).toString() == normalizedTopic) {
            hasTopic = true;
            break;
        }
    }
    if (!hasTopic) {
        return;
    }

    const QString action = m_platformActions.showSubscriptionContextMenu(globalPosition);
    if (action == QStringLiteral("edit")) {
        emit subscriptionEditRequested(filteredIndex);
    } else if (action == QStringLiteral("delete")) {
        const QVariantMap subscription = filteredSubscriptions() ? filteredSubscriptions()->rowAt(filteredIndex) : QVariantMap {};
        requestSubscriptionDelete(
            topic,
            subscription.value(QStringLiteral("displayName"), topic).toString());
    }
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

void WorkbenchViewModel::useMessageAsPublishDraft(const QString &topic, const QString &payload, const QString &testPayload, int format)
{
    setPublishTopic(topic);
    setPublishPayload(testPayload.isEmpty() ? payload : testPayload);
    if (format >= 0) {
        setPublishFormat(format);
    }
}

bool WorkbenchViewModel::publishDraft()
{
    if (!canPublish()) {
        return false;
    }

    if (!m_dependencies.mqttController) {
        return false;
    }

    m_dependencies.mqttController->publishCurrentSession(
        m_publishTopic.trimmed(),
        m_publishPayload,
        m_publishFormat,
        m_publishQos,
        m_publishRetain);
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

QString WorkbenchViewModel::normalizedSubscriptionFilterMode(const QString &filterMode)
{
    return filterMode == QStringLiteral("subscribed") || filterMode == QStringLiteral("paused")
        ? filterMode
        : QStringLiteral("all");
}

int WorkbenchViewModel::subscriptionFilterModeIndexForMode(const QString &filterMode)
{
    const QString normalizedMode = normalizedSubscriptionFilterMode(filterMode);
    if (normalizedMode == QStringLiteral("subscribed")) {
        return 1;
    }
    if (normalizedMode == QStringLiteral("paused")) {
        return 2;
    }
    return 0;
}

ScriptLibraryModel *WorkbenchViewModel::scriptLibrary() const
{
    return m_dependencies.scripts;
}

void WorkbenchViewModel::syncSubscriptionFilterModel()
{
    if (!filteredSubscriptions()) {
        return;
    }

    filteredSubscriptions()->setFilterText(m_subscriptionFilterText);
    filteredSubscriptions()->setFilterMode(m_subscriptionFilterMode);
}

void WorkbenchViewModel::emitSubscriptionFilterSignals(const QString &oldText, const QString &oldMode)
{
    if (oldText != m_subscriptionFilterText) {
        emit subscriptionFilterTextChanged();
    }
    if (oldMode != m_subscriptionFilterMode) {
        emit subscriptionFilterModeChanged();
        emit subscriptionFilterModeIndexChanged();
    }
    if ((!oldText.trimmed().isEmpty() || oldMode != QStringLiteral("all")) != hasSubscriptionFilter()) {
        emit subscriptionFilterChanged();
    }
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
