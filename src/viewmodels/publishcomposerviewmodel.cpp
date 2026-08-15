#include "viewmodels/publishcomposerviewmodel.h"

#include "domain/sessionconfig.h"
#include "models/draftlibrarymodel.h"
#include "services/apputils.h"
#include "services/payload/payloadcodec.h"
#include "usecases/draftlibraryservice.h"
#include "usecases/mqttsessionservice.h"
#include "usecases/sessionservice.h"

using namespace AppUtils;

namespace {
bool isValidBase64(const QString &text)
{
    const QByteArray encoded = text.trimmed().toLatin1();
    return encoded.isEmpty()
        || QByteArray::fromBase64Encoding(
               encoded,
               QByteArray::AbortOnBase64DecodingErrors);
}
}

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
bool PublishComposerViewModel::payloadUtf8() const { return m_payloadUtf8; }
QString PublishComposerViewModel::messageExpiryText() const { return m_messageExpiryText; }
QString PublishComposerViewModel::topicAliasText() const { return m_topicAliasText; }
QString PublishComposerViewModel::responseTopic() const { return m_responseTopic; }
QString PublishComposerViewModel::correlationDataBase64() const { return m_correlationDataBase64; }
QString PublishComposerViewModel::contentType() const { return m_contentType; }
QString PublishComposerViewModel::userPropertiesText() const { return m_userPropertiesText; }
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
        && !m_topic.trimmed().isEmpty()
        && isValidBase64(m_correlationDataBase64);
}

bool PublishComposerViewModel::hasContent() const
{
    return !m_topic.trimmed().isEmpty()
        || !m_payload.isEmpty()
        || m_format != 1
        || m_qos != 0
        || m_retain
        || !collectedProperties().isEmpty();
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

void PublishComposerViewModel::setPayloadUtf8(bool enabled)
{
    if (m_payloadUtf8 == enabled) return;
    m_payloadUtf8 = enabled;
    emit propertiesChanged();
    emit composerStateChanged();
}

void PublishComposerViewModel::setMessageExpiryText(const QString &text)
{
    if (m_messageExpiryText == text) return;
    m_messageExpiryText = text;
    emit propertiesChanged();
    emit composerStateChanged();
}

void PublishComposerViewModel::setTopicAliasText(const QString &text)
{
    if (m_topicAliasText == text) return;
    m_topicAliasText = text;
    emit propertiesChanged();
    emit composerStateChanged();
}

void PublishComposerViewModel::setResponseTopic(const QString &topic)
{
    if (m_responseTopic == topic) return;
    m_responseTopic = topic;
    emit propertiesChanged();
    emit composerStateChanged();
}

void PublishComposerViewModel::setCorrelationDataBase64(const QString &data)
{
    if (m_correlationDataBase64 == data) return;
    const bool wasPublishable = canPublish();
    m_correlationDataBase64 = data;
    emit propertiesChanged();
    emit composerStateChanged();
    if (wasPublishable != canPublish()) emit canPublishChanged();
}

void PublishComposerViewModel::setContentType(const QString &contentType)
{
    if (m_contentType == contentType) return;
    m_contentType = contentType;
    emit propertiesChanged();
    emit composerStateChanged();
}

void PublishComposerViewModel::setUserPropertiesText(const QString &text)
{
    if (m_userPropertiesText == text) return;
    m_userPropertiesText = text;
    emit propertiesChanged();
    emit composerStateChanged();
}

MqttPublishProperties PublishComposerViewModel::collectedProperties() const
{
    MqttPublishProperties properties;
    if (m_payloadUtf8) {
        properties.payloadFormatIndicator = MqttPayloadFormatIndicator::Utf8;
    }
    if (!m_messageExpiryText.trimmed().isEmpty()) {
        properties.messageExpiryInterval = SessionConfig::sanitizeOptionalUInt32(
            m_messageExpiryText);
    }
    if (!m_topicAliasText.trimmed().isEmpty()) {
        properties.topicAlias = SessionConfig::sanitizeOptionalUInt16(m_topicAliasText);
    }
    properties.responseTopic = m_responseTopic.trimmed();
    properties.correlationData = QByteArray::fromBase64(m_correlationDataBase64.trimmed().toLatin1());
    properties.contentType = m_contentType.trimmed();
    properties.userProperties = mqttUserPropertiesFromText(m_userPropertiesText);
    return properties;
}

void PublishComposerViewModel::loadProperties(const MqttPublishProperties &properties)
{
    setPayloadUtf8(properties.payloadFormatIndicator == MqttPayloadFormatIndicator::Utf8);
    setMessageExpiryText(properties.messageExpiryInterval
            ? QString::number(*properties.messageExpiryInterval)
            : QString());
    setTopicAliasText(properties.topicAlias ? QString::number(*properties.topicAlias) : QString());
    setResponseTopic(properties.responseTopic);
    setCorrelationDataBase64(QString::fromLatin1(properties.correlationData.toBase64()));
    setContentType(properties.contentType);
    setUserPropertiesText(mqttUserPropertiesToText(properties.userProperties));
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
    loadProperties(mqttPublishPropertiesFromBase64Cbor(
                       entry.value(QStringLiteral("propertiesCborBase64")).toString())
                       .value_or(MqttPublishProperties {}));
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
        mqttPublishPropertiesFromBase64Cbor(
            entry.value(QStringLiteral("propertiesCborBase64")).toString())
            .value_or(MqttPublishProperties {}),
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
            m_topic.trimmed(),
            m_payload,
            m_format,
            m_qos,
            m_retain,
            collectedProperties(),
            tr("Publish composer"));
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
    loadProperties(mqttPublishPropertiesFromBase64Cbor(
                       row.value(QStringLiteral("propertiesCborBase64")).toString())
                       .value_or(MqttPublishProperties {}));
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
        mqttPublishPropertiesFromBase64Cbor(
            row.value(QStringLiteral("propertiesCborBase64")).toString())
            .value_or(MqttPublishProperties {}),
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
        || m_retain != row.value(QStringLiteral("retain"), false).toBool()
        || collectedProperties() != mqttPublishPropertiesFromBase64Cbor(
                                        row.value(QStringLiteral("propertiesCborBase64")).toString())
                                        .value_or(MqttPublishProperties {});
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
    draft.properties = collectedProperties();
    return m_draftService.createDraft(draft);
}
