#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

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

public:
    struct Dependencies
    {
        std::function<void(QObject *, std::function<void()>)> bindPublishAvailabilityChanged;
        std::function<bool()> canPublishToCurrentSession;
        std::function<void(const QString &, const QString &, int, int, bool)> publishCurrentSession;
    };

    explicit PublishDraftViewModel(QObject *parent = nullptr);
    explicit PublishDraftViewModel(const Dependencies &dependencies, QObject *parent = nullptr);

    QStringList payloadFormats() const;
    QString topic() const;
    QString payload() const;
    int format() const;
    int qos() const;
    bool retain() const;
    bool canPublish() const;

    void setTopic(const QString &topic);
    void setPayload(const QString &payload);
    void setFormat(int format);
    void setQos(int qos);
    void setRetain(bool retain);

    Q_INVOKABLE void useMessageAsDraft(const QString &topic, const QString &payload, const QString &testPayload, int format);
    Q_INVOKABLE bool publishDraft();

signals:
    void topicChanged();
    void payloadChanged();
    void formatChanged();
    void qosChanged();
    void retainChanged();
    void canPublishChanged();

private:
    Dependencies m_dependencies;
    QString m_topic;
    QString m_payload;
    int m_format = 1;
    int m_qos = 0;
    bool m_retain = false;
};
