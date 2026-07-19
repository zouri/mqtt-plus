#include "viewmodels/publishdraftviewmodel.h"

#include "services/payload/payloadcodec.h"

#include <QDateTime>

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
QVariantList PublishDraftViewModel::recentPublishes() const { return m_recentPublishes; }

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

bool PublishDraftViewModel::useRecentPublish(int index)
{
    if (index < 0 || index >= m_recentPublishes.size()) {
        return false;
    }

    const QVariantMap entry = m_recentPublishes.at(index).toMap();
    setTopic(entry.value(QStringLiteral("topic")).toString());
    setPayload(entry.value(QStringLiteral("payload")).toString());
    setFormat(entry.value(QStringLiteral("format"), m_format).toInt());
    setQos(entry.value(QStringLiteral("qos"), m_qos).toInt());
    setRetain(entry.value(QStringLiteral("retain"), m_retain).toBool());
    return true;
}

void PublishDraftViewModel::clearRecentPublishes()
{
    if (m_recentPublishes.isEmpty()) {
        return;
    }
    m_recentPublishes.clear();
    emit recentPublishesChanged();
}

bool PublishDraftViewModel::publishDraft()
{
    if (!canPublish() || !m_dependencies.publishCurrentSession) {
        return false;
    }

    const QString topic = m_topic.trimmed();
    if (!m_dependencies.publishCurrentSession(
            topic,
            m_payload,
            m_format,
            m_qos,
            m_retain)) {
        return false;
    }

    QVariantMap entry {
        {QStringLiteral("topic"), topic},
        {QStringLiteral("payload"), m_payload},
        {QStringLiteral("format"), m_format},
        {QStringLiteral("formatName"), PayloadCodec::formatName(PayloadCodec::formatFromInt(m_format))},
        {QStringLiteral("qos"), m_qos},
        {QStringLiteral("retain"), m_retain},
        {QStringLiteral("publishedAt"), QDateTime::currentDateTime().toString(Qt::ISODate)},
    };
    for (qsizetype index = m_recentPublishes.size() - 1; index >= 0; --index) {
        const QVariantMap existing = m_recentPublishes.at(index).toMap();
        if (existing.value(QStringLiteral("topic")) == topic
            && existing.value(QStringLiteral("payload")) == m_payload
            && existing.value(QStringLiteral("format")) == m_format
            && existing.value(QStringLiteral("qos")) == m_qos
            && existing.value(QStringLiteral("retain")) == m_retain) {
            m_recentPublishes.removeAt(index);
        }
    }
    m_recentPublishes.prepend(entry);
    while (m_recentPublishes.size() > 10) {
        m_recentPublishes.removeLast();
    }
    emit recentPublishesChanged();
    return true;
}
