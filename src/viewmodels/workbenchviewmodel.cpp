#include "viewmodels/workbenchviewmodel.h"

#include "viewmodels/workbenchcoreport.h"

WorkbenchViewModel::WorkbenchViewModel(WorkbenchCorePort *core, QObject *parent)
    : QObject(parent)
    , m_core(core)
{
    if (!m_core) {
        return;
    }

    WorkbenchCoreSignalHandlers handlers;
    handlers.currentSessionIndexChanged = [this]() {
        emit currentSessionIndexChanged();
    };
    handlers.currentSessionChanged = [this]() {
        emit currentSessionChanged();
        emit canPublishChanged();
    };
    handlers.messageStreamChanged = [this]() {
        emit messageStreamChanged();
    };
    handlers.messageStreamRowAppended = [this]() {
        emit messageStreamRowAppended();
    };
    handlers.scriptLibraryChanged = [this]() {
        refreshSubscriptionEditorScriptOptions();
    };
    m_core->bindWorkbenchSignals(this, handlers);
    syncSubscriptionFilterModel();
    refreshSubscriptionEditorScriptOptions();
}

SessionListModel *WorkbenchViewModel::sessions() const { return m_core ? m_core->sessions() : nullptr; }
SubscriptionFilterModel *WorkbenchViewModel::filteredSubscriptions() const { return m_core ? m_core->filteredSubscriptions() : nullptr; }
EventStreamModel *WorkbenchViewModel::messages() const { return m_core ? m_core->messages() : nullptr; }
SessionEditorViewModel *WorkbenchViewModel::sessionEditor() { return &m_sessionEditor; }
SubscriptionEditorViewModel *WorkbenchViewModel::subscriptionEditor() { return &m_subscriptionEditor; }
int WorkbenchViewModel::currentSessionIndex() const { return m_core ? m_core->currentSessionIndex() : -1; }
QVariantMap WorkbenchViewModel::currentSession() const { return m_core ? m_core->currentSession() : QVariantMap {}; }
QVariantMap WorkbenchViewModel::sessionStatus() const { return m_core ? m_core->sessionStatus() : QVariantMap {}; }
QVariantMap WorkbenchViewModel::publishStatus() const { return m_core ? m_core->publishStatus() : QVariantMap {}; }
QStringList WorkbenchViewModel::payloadFormats() const { return m_core ? m_core->payloadFormats() : QStringList {}; }
QString WorkbenchViewModel::publishTopic() const { return m_publishTopic; }
QString WorkbenchViewModel::publishPayload() const { return m_publishPayload; }
int WorkbenchViewModel::publishFormat() const { return m_publishFormat; }
int WorkbenchViewModel::publishQos() const { return m_publishQos; }
bool WorkbenchViewModel::publishRetain() const { return m_publishRetain; }
bool WorkbenchViewModel::canPublish() const
{
    return m_core
        && m_core->sessionStatus().value(QStringLiteral("state")).toString() == QStringLiteral("connected")
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
    if (m_core) {
        m_core->setCurrentSessionIndex(index);
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
    m_sessionEditor.openForCreate(m_core ? m_core->defaultSessionConfig() : SessionEditorViewModel::defaultConfig(1));
}

void WorkbenchViewModel::openSessionEditorForEdit(int index)
{
    if (!m_core || index < 0 || index >= m_core->sessions()->count()) {
        return;
    }
    m_sessionEditor.openForEdit(index, m_core->sessionConfigAt(index));
}

bool WorkbenchViewModel::submitSessionEditor()
{
    if (!m_sessionEditor.validate()) {
        return false;
    }
    if (!m_core) {
        return false;
    }

    const QVariantMap config = m_sessionEditor.collectedConfig();
    if (!m_sessionEditor.editMode()) {
        m_core->addSessionWithConfig(config);
        return true;
    }

    const int index = m_sessionEditor.targetIndex();
    return index >= 0 && m_core->updateSessionConfigAt(index, config);
}

void WorkbenchViewModel::handleSessionContextMenu(int index, const QPointF &globalPosition)
{
    if (!m_core) {
        return;
    }

    const QString action = m_core->showSessionContextMenu(index, globalPosition);
    if (action == QStringLiteral("edit")) {
        emit sessionEditRequested(index);
    } else if (action == QStringLiteral("copy")) {
        m_core->duplicateSessionAt(index);
    } else if (action == QStringLiteral("delete")) {
        m_core->removeSessionAt(index);
    }
}

void WorkbenchViewModel::handleSubscriptionContextMenu(int filteredIndex, const QString &topic, const QPointF &globalPosition)
{
    if (!m_core) {
        return;
    }

    const QString action = m_core->showSubscriptionContextMenu(topic, globalPosition);
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
    if (!m_core) {
        return;
    }

    const QString state = m_core->sessionStatus().value(QStringLiteral("state")).toString();
    if (state == QStringLiteral("connected")
        || state == QStringLiteral("connecting")
        || state == QStringLiteral("disconnecting")) {
        m_core->disconnectCurrentSession();
        return;
    }

    m_core->connectCurrentSession();
}

void WorkbenchViewModel::toggleCurrentOutputPaused(bool currentlyPaused)
{
    if (m_core) {
        m_core->setCurrentOutputPaused(!currentlyPaused);
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
    if (!m_core || !filteredSubscriptions() || filteredIndex < 0 || filteredIndex >= filteredSubscriptions()->count()) {
        return false;
    }

    refreshSubscriptionEditorScriptOptions();
    m_subscriptionEditor.openForEdit(filteredSubscriptions()->rowAt(filteredIndex));
    return true;
}

bool WorkbenchViewModel::submitSubscriptionEditor()
{
    if (!m_core || !m_subscriptionEditor.canSubmit()) {
        return false;
    }

    const QVariantMap submission = m_subscriptionEditor.submission();
    if (submission.value(QStringLiteral("editMode")).toBool()) {
        return m_core->updateCurrentSubscription(
            submission.value(QStringLiteral("editTopic")).toString(),
            submission.value(QStringLiteral("topic")).toString(),
            submission.value(QStringLiteral("alias")).toString(),
            submission.value(QStringLiteral("scriptId")).toString());
    }

    return m_core->upsertCurrentSubscription(
        submission.value(QStringLiteral("topic")).toString(),
        submission.value(QStringLiteral("qos")).toInt(),
        submission.value(QStringLiteral("format")).toInt(),
        submission.value(QStringLiteral("scriptId")).toString(),
        submission.value(QStringLiteral("alias")).toString());
}
void WorkbenchViewModel::toggleCurrentSubscriptionPaused(const QString &topic, bool currentlyPaused)
{
    if (m_core) {
        m_core->setCurrentSubscriptionPaused(topic, !currentlyPaused);
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
    if (topic.isEmpty() || !m_core) {
        clearPendingSubscriptionDelete();
        return false;
    }

    m_core->removeCurrentSubscription(topic);
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

    m_core->publishCurrentSession(
        m_publishTopic.trimmed(),
        m_publishPayload,
        m_publishFormat,
        m_publishQos,
        m_publishRetain);
    return true;
}

void WorkbenchViewModel::copyMessageTopic(const QString &topic) const
{
    if (m_core) {
        m_core->copyTextToClipboard(topic);
    }
}

void WorkbenchViewModel::copyMessagePayload(const QString &payload, const QString &testPayload) const
{
    if (m_core) {
        m_core->copyTextToClipboard(testPayload.isEmpty() ? payload : testPayload);
    }
}

void WorkbenchViewModel::clearMessages()
{
    if (m_core) {
        m_core->clearCurrentMessages();
    }
}

int WorkbenchViewModel::loadOlderMessages()
{
    return m_core ? m_core->loadOlderCurrentSessionMessages() : 0;
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
    return m_core ? m_core->scripts() : nullptr;
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
