#include "viewmodels/publishcomposerviewmodel.h"

#include "domain/sessionconfig.h"
#include "models/draftlibrarymodel.h"
#include "services/apputils.h"
#include "services/payload/payloadcodec.h"
#include "usecases/draftlibraryservice.h"
#include "usecases/mqttsessionservice.h"
#include "usecases/sessionservice.h"

using namespace AppUtils;

PublishComposerViewModel::PublishComposerViewModel(
    SessionService &sessionService,
    MqttSessionService &mqttService,
    DraftLibraryService &draftService,
    DraftLibraryModel &draftsModel,
    QObject *parent)
    : QObject(parent)
    , m_sessionService(sessionService)
    , m_mqttService(mqttService)
    , m_draftService(draftService)
    , m_drafts(this)
{
    m_drafts.setSourceModel(&draftsModel);
    m_drafts.setSortMode(QStringLiteral("recent"));
    connect(&m_sessionService, &SessionService::currentSessionChanged,
            this, &PublishComposerViewModel::canPublishChanged);
    connect(&m_mqttService, &MqttSessionService::sessionStateChanged,
            this, &PublishComposerViewModel::canPublishChanged);
    connect(&m_mqttService, &MqttSessionService::recentPublishesChanged,
            this, &PublishComposerViewModel::recentPublishesChanged);
    connect(&m_draftService, &DraftLibraryService::stateChanged,
            this, &PublishComposerViewModel::draftLibraryStateChanged);
}

QStringList PublishComposerViewModel::payloadFormats() const { return PayloadCodec::formatNames(); }
QString PublishComposerViewModel::topic() const { return m_topic; }
QString PublishComposerViewModel::payload() const { return m_payload; }
int PublishComposerViewModel::format() const { return m_format; }
int PublishComposerViewModel::qos() const { return m_qos; }
bool PublishComposerViewModel::retain() const { return m_retain; }
QVariantList PublishComposerViewModel::recentPublishes() const { return m_mqttService.recentPublishes(); }
DraftFilterModel *PublishComposerViewModel::drafts() { return &m_drafts; }
bool PublishComposerViewModel::draftsLoading() const { return m_draftService.loading(); }
bool PublishComposerViewModel::draftsReady() const { return m_draftService.ready(); }
bool PublishComposerViewModel::draftsBusy() const { return m_draftService.busy(); }
QString PublishComposerViewModel::draftError() const { return m_draftService.errorMessage(); }

bool PublishComposerViewModel::canPublish() const
{
    const SessionState *session = m_sessionService.currentSession();
    return session
        && sessionStateName(*session, session->runtime.client) == QStringLiteral("connected")
        && !m_topic.trimmed().isEmpty();
}

bool PublishComposerViewModel::hasContent() const
{
    return !m_topic.trimmed().isEmpty() || !m_payload.isEmpty() || m_format != 1 || m_qos != 0 || m_retain;
}

void PublishComposerViewModel::setTopic(const QString &topic)
{
    if (m_topic == topic) return;
    const bool wasPublishable = canPublish();
    m_topic = topic;
    emit topicChanged();
    emit composerStateChanged();
    if (wasPublishable != canPublish()) emit canPublishChanged();
}

void PublishComposerViewModel::setPayload(const QString &payload)
{
    if (m_payload == payload) return;
    m_payload = payload;
    emit payloadChanged();
    emit composerStateChanged();
}

void PublishComposerViewModel::setFormat(int format)
{
    if (m_format == format) return;
    m_format = format;
    emit formatChanged();
    emit composerStateChanged();
}

void PublishComposerViewModel::setQos(int qos)
{
    const int normalized = SessionConfig::sanitizeQos(qos);
    if (m_qos == normalized) return;
    m_qos = normalized;
    emit qosChanged();
    emit composerStateChanged();
}

void PublishComposerViewModel::setRetain(bool retain)
{
    if (m_retain == retain) return;
    m_retain = retain;
    emit retainChanged();
    emit composerStateChanged();
}

void PublishComposerViewModel::useMessageAsDraft(
    const QString &topic,
    const QString &payload,
    const QString &testPayload,
    int format)
{
    setTopic(topic);
    setPayload(testPayload.isEmpty() ? payload : testPayload);
    if (format >= 0) setFormat(format);
}

bool PublishComposerViewModel::useRecentPublish(int index)
{
    const QVariantList recent = m_mqttService.recentPublishes();
    if (index < 0 || index >= recent.size()) return false;
    const QVariantMap entry = recent.at(index).toMap();
    setTopic(entry.value(QStringLiteral("topic")).toString());
    setPayload(entry.value(QStringLiteral("payload")).toString());
    setFormat(entry.value(QStringLiteral("format"), m_format).toInt());
    setQos(entry.value(QStringLiteral("qos"), m_qos).toInt());
    setRetain(entry.value(QStringLiteral("retain"), m_retain).toBool());
    return true;
}

bool PublishComposerViewModel::quickPublishRecent(int index)
{
    const QVariantList recent = m_mqttService.recentPublishes();
    if (index < 0 || index >= recent.size()) return false;
    const QVariantMap entry = recent.at(index).toMap();
    return m_mqttService.publishCurrentSession(
        entry.value(QStringLiteral("topic")).toString(),
        entry.value(QStringLiteral("payload")).toString(),
        entry.value(QStringLiteral("format"), 1).toInt(),
        entry.value(QStringLiteral("qos"), 0).toInt(),
        entry.value(QStringLiteral("retain"), false).toBool(),
        tr("Recent publish"));
}

void PublishComposerViewModel::clearRecentPublishes()
{
    m_mqttService.clearRecentPublishes();
}

bool PublishComposerViewModel::publishDraft()
{
    return canPublish()
        && m_mqttService.publishCurrentSession(
            m_topic.trimmed(), m_payload, m_format, m_qos, m_retain, tr("Publish composer"));
}

void PublishComposerViewModel::setDraftFilterText(const QString &text)
{
    m_drafts.setFilterText(text);
}

int PublishComposerViewModel::draftIndexOfId(const QString &id) const
{
    for (int row = 0; row < m_drafts.rowCount(); ++row) {
        if (m_drafts.rowAt(row).value(QStringLiteral("id")).toString() == id) {
            return row;
        }
    }
    return -1;
}

bool PublishComposerViewModel::useSavedDraft(int index)
{
    const QVariantMap row = m_drafts.rowAt(index);
    if (row.isEmpty()) return false;
    setTopic(row.value(QStringLiteral("defaultTopic")).toString());
    setPayload(row.value(QStringLiteral("payload")).toString());
    setFormat(row.value(QStringLiteral("format"), 1).toInt());
    setQos(row.value(QStringLiteral("qos"), 0).toInt());
    setRetain(row.value(QStringLiteral("retain"), false).toBool());
    m_draftService.markUsed(row.value(QStringLiteral("id")).toString());
    return true;
}

bool PublishComposerViewModel::quickPublishDraft(int index, const QString &temporaryTopic)
{
    const QVariantMap row = m_drafts.rowAt(index);
    if (row.isEmpty()) return false;
    QString topic = row.value(QStringLiteral("defaultTopic")).toString().trimmed();
    if (topic.isEmpty()) topic = temporaryTopic.trimmed();
    const bool sent = m_mqttService.publishCurrentSession(
        topic,
        row.value(QStringLiteral("payload")).toString(),
        row.value(QStringLiteral("format"), 1).toInt(),
        row.value(QStringLiteral("qos"), 0).toInt(),
        row.value(QStringLiteral("retain"), false).toBool(),
        row.value(QStringLiteral("name")).toString());
    if (sent) m_draftService.markUsed(row.value(QStringLiteral("id")).toString());
    return sent;
}

bool PublishComposerViewModel::draftNeedsTopic(int index) const
{
    return m_drafts.rowAt(index).value(QStringLiteral("defaultTopic")).toString().trimmed().isEmpty();
}

bool PublishComposerViewModel::wouldReplaceWithDraft(int index) const
{
    if (!hasContent()) return false;
    const QVariantMap row = m_drafts.rowAt(index);
    if (row.isEmpty()) return false;
    return m_topic != row.value(QStringLiteral("defaultTopic")).toString()
        || m_payload != row.value(QStringLiteral("payload")).toString()
        || m_format != row.value(QStringLiteral("format"), 1).toInt()
        || m_qos != row.value(QStringLiteral("qos"), 0).toInt()
        || m_retain != row.value(QStringLiteral("retain"), false).toBool();
}

bool PublishComposerViewModel::saveAsDraft(const QString &name)
{
    PublishDraft draft;
    draft.name = name;
    draft.defaultTopic = m_topic;
    draft.payload = m_payload;
    draft.formatId = PayloadCodec::formatId(PayloadCodec::formatFromInt(m_format));
    draft.qos = m_qos;
    draft.retain = m_retain;
    return m_draftService.createDraft(draft);
}
