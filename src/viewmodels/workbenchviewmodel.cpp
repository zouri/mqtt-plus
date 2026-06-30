#include "viewmodels/workbenchviewmodel.h"

#include "app/applicationcore.h"

WorkbenchViewModel::WorkbenchViewModel(ApplicationCore *core, QObject *parent)
    : QObject(parent)
    , m_core(core)
{
    if (!m_core) {
        return;
    }

    connect(m_core, &ApplicationCore::currentSessionIndexChanged, this, &WorkbenchViewModel::currentSessionIndexChanged);
    connect(m_core, &ApplicationCore::currentSessionChanged, this, &WorkbenchViewModel::currentSessionChanged);
    connect(m_core, &ApplicationCore::subscriptionsChanged, this, &WorkbenchViewModel::subscriptionsChanged);
    connect(m_core, &ApplicationCore::messageStreamChanged, this, &WorkbenchViewModel::messageStreamChanged);
    connect(m_core, &ApplicationCore::logStreamChanged, this, &WorkbenchViewModel::logStreamChanged);
    connect(m_core, &ApplicationCore::messageStreamRowAppended, this, &WorkbenchViewModel::messageStreamRowAppended);
    connect(m_core, &ApplicationCore::logStreamRowAppended, this, &WorkbenchViewModel::logStreamRowAppended);
    connect(m_core, &ApplicationCore::scriptLibraryChanged, this, [this]() {
        refreshSubscriptionEditorScriptOptions();
        emit scriptLibraryChanged();
    });
    connect(m_core, &ApplicationCore::currentSessionChanged, this, &WorkbenchViewModel::canPublishChanged);
    refreshSubscriptionEditorScriptOptions();
}

SessionListModel *WorkbenchViewModel::sessions() const { return m_core ? m_core->sessions() : nullptr; }
SubscriptionListModel *WorkbenchViewModel::subscriptions() const { return m_core ? m_core->subscriptions() : nullptr; }
SubscriptionFilterModel *WorkbenchViewModel::filteredSubscriptions() const { return m_core ? m_core->filteredSubscriptions() : nullptr; }
EventStreamModel *WorkbenchViewModel::messages() const { return m_core ? m_core->messages() : nullptr; }
ScriptLibraryModel *WorkbenchViewModel::scripts() const { return m_core ? m_core->scripts() : nullptr; }
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

void WorkbenchViewModel::duplicateSessionAt(int index) { if (m_core) { m_core->duplicateSessionAt(index); } }
void WorkbenchViewModel::removeSessionAt(int index) { if (m_core) { m_core->removeSessionAt(index); } }
QString WorkbenchViewModel::showSessionContextMenu(int index, const QPointF &globalPosition) { return m_core ? m_core->showSessionContextMenu(index, globalPosition) : QString(); }
QString WorkbenchViewModel::showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition) { return m_core ? m_core->showSubscriptionContextMenu(topic, globalPosition) : QString(); }
void WorkbenchViewModel::connectCurrentSession() { if (m_core) { m_core->connectCurrentSession(); } }
void WorkbenchViewModel::disconnectCurrentSession() { if (m_core) { m_core->disconnectCurrentSession(); } }
void WorkbenchViewModel::setCurrentOutputPaused(bool paused) { if (m_core) { m_core->setCurrentOutputPaused(paused); } }
void WorkbenchViewModel::refreshSubscriptionEditorScriptOptions()
{
    QVariantList options;
    if (auto *scriptModel = scripts()) {
        for (int row = 0; row < scriptModel->rowCount(); ++row) {
            options.append(scriptModel->rowAt(row));
        }
    }
    m_subscriptionEditor.setScriptOptions(options);
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
void WorkbenchViewModel::removeCurrentSubscription(const QString &topic) { if (m_core) { m_core->removeCurrentSubscription(topic); } }
void WorkbenchViewModel::setCurrentSubscriptionPaused(const QString &topic, bool paused) { if (m_core) { m_core->setCurrentSubscriptionPaused(topic, paused); } }
void WorkbenchViewModel::setPublishDraft(const QString &topic, const QString &payload, int format)
{
    setPublishTopic(topic);
    setPublishPayload(payload);
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

void WorkbenchViewModel::copyTextToClipboard(const QString &text) const { if (m_core) { m_core->copyTextToClipboard(text); } }
void WorkbenchViewModel::clearCurrentMessages() { if (m_core) { m_core->clearCurrentMessages(); } }
int WorkbenchViewModel::loadOlderCurrentSessionMessages() { return m_core ? m_core->loadOlderCurrentSessionMessages() : 0; }
