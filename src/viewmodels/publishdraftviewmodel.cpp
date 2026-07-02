#include "viewmodels/publishdraftviewmodel.h"

#include "services/payload/payloadcodec.h"

PublishDraftViewModel::PublishDraftViewModel(QObject *parent)
    : PublishDraftViewModel(Dependencies {}, parent)
{
}

PublishDraftViewModel::PublishDraftViewModel(const Dependencies &dependencies, QObject *parent)
    : QObject(parent)
    , m_dependencies(dependencies)
{
    if (m_dependencies.bindPublishAvailabilityChanged) {
        m_dependencies.bindPublishAvailabilityChanged(this, [this]() {
            emit canPublishChanged();
        });
    }
}

QStringList PublishDraftViewModel::payloadFormats() const { return PayloadCodec::formatNames(); }
QString PublishDraftViewModel::topic() const { return m_topic; }
QString PublishDraftViewModel::payload() const { return m_payload; }
int PublishDraftViewModel::format() const { return m_format; }
int PublishDraftViewModel::qos() const { return m_qos; }
bool PublishDraftViewModel::retain() const { return m_retain; }

bool PublishDraftViewModel::canPublish() const
{
    return m_dependencies.canPublishToCurrentSession
        && m_dependencies.canPublishToCurrentSession()
        && !m_topic.trimmed().isEmpty();
}

void PublishDraftViewModel::setTopic(const QString &topic)
{
    if (m_topic == topic) {
        return;
    }

    const bool wasPublishable = canPublish();
    m_topic = topic;
    emit topicChanged();
    if (wasPublishable != canPublish()) {
        emit canPublishChanged();
    }
}

void PublishDraftViewModel::setPayload(const QString &payload)
{
    if (m_payload == payload) {
        return;
    }

    m_payload = payload;
    emit payloadChanged();
}

void PublishDraftViewModel::setFormat(int format)
{
    if (m_format == format) {
        return;
    }

    m_format = format;
    emit formatChanged();
}

void PublishDraftViewModel::setQos(int qos)
{
    if (m_qos == qos) {
        return;
    }

    m_qos = qos;
    emit qosChanged();
}

void PublishDraftViewModel::setRetain(bool retain)
{
    if (m_retain == retain) {
        return;
    }

    m_retain = retain;
    emit retainChanged();
}

void PublishDraftViewModel::useMessageAsDraft(const QString &topic, const QString &payload, const QString &testPayload, int format)
{
    setTopic(topic);
    setPayload(testPayload.isEmpty() ? payload : testPayload);
    if (format >= 0) {
        setFormat(format);
    }
}

bool PublishDraftViewModel::publishDraft()
{
    if (!canPublish() || !m_dependencies.publishCurrentSession) {
        return false;
    }

    m_dependencies.publishCurrentSession(
        m_topic.trimmed(),
        m_payload,
        m_format,
        m_qos,
        m_retain);
    return true;
}
