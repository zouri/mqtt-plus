#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class MqttSessionService;
class SessionService;

class PublishDraftViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList payloadFormats READ payloadFormats CONSTANT)
    Q_PROPERTY(QString topic READ topic WRITE setTopic NOTIFY topicChanged)
    Q_PROPERTY(QString payload READ payload WRITE setPayload NOTIFY payloadChanged)
    Q_PROPERTY(int format READ format WRITE setFormat NOTIFY formatChanged)
    Q_PROPERTY(int qos READ qos WRITE setQos NOTIFY qosChanged)
    Q_PROPERTY(bool retain READ retain WRITE setRetain NOTIFY retainChanged)
    Q_PROPERTY(bool canPublish READ canPublish NOTIFY canPublishChanged)
    Q_PROPERTY(QVariantList recentPublishes READ recentPublishes NOTIFY recentPublishesChanged)

public:
    explicit PublishDraftViewModel(
        SessionService &sessionService,
        MqttSessionService &mqttService,
        QObject *parent = nullptr);

    QStringList payloadFormats() const;
    QString topic() const;
    QString payload() const;
    int format() const;
    int qos() const;
    bool retain() const;
    bool canPublish() const;
    QVariantList recentPublishes() const;

    void setTopic(const QString &topic);
    void setPayload(const QString &payload);
    void setFormat(int format);
    void setQos(int qos);
    void setRetain(bool retain);

    Q_INVOKABLE void useMessageAsDraft(const QString &topic, const QString &payload, const QString &testPayload, int format);
    Q_INVOKABLE bool useRecentPublish(int index);
    Q_INVOKABLE void clearRecentPublishes();
    Q_INVOKABLE bool publishDraft();

signals:
    void topicChanged();
    void payloadChanged();
    void formatChanged();
    void qosChanged();
    void retainChanged();
    void canPublishChanged();
    void recentPublishesChanged();

private:
    SessionService &m_sessionService;
    MqttSessionService &m_mqttService;
    QString m_topic;
    QString m_payload;
    int m_format = 1;
    int m_qos = 0;
    bool m_retain = false;
    QVariantList m_recentPublishes;
};
