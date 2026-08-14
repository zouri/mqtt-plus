#pragma once

#include "domain/subscription.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

struct SubscriptionEditorSubmission
{
    bool editMode = false;
    QString editTopic;
    QString topic;
    QStringList topics;
    QString alias;
    int qos = 0;
    int format = 0;
    ProcessorReference processor;
    QString color;
};

class SubscriptionEditorViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool editMode READ editMode NOTIFY editModeChanged)
    Q_PROPERTY(QString editTopic READ editTopic NOTIFY editTopicChanged)
    Q_PROPERTY(QString topic READ topic WRITE setTopic NOTIFY topicChanged)
    Q_PROPERTY(QString alias READ alias WRITE setAlias NOTIFY aliasChanged)
    Q_PROPERTY(int qos READ qos WRITE setQos NOTIFY qosChanged)
    Q_PROPERTY(int format READ format WRITE setFormat NOTIFY formatChanged)
    Q_PROPERTY(QString processorId READ processorId WRITE setProcessorId NOTIFY processorIdChanged)
    Q_PROPERTY(int processorIndex READ processorIndex WRITE setProcessorIndex NOTIFY processorIndexChanged)
    Q_PROPERTY(QStringList processorOptionIds READ processorOptionIds NOTIFY processorOptionsChanged)
    Q_PROPERTY(QStringList processorOptionNames READ processorOptionNames NOTIFY processorOptionsChanged)
    Q_PROPERTY(QString processorBindingDetail READ processorBindingDetail NOTIFY processorBindingDetailChanged)
    Q_PROPERTY(QString color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QStringList colorOptions READ colorOptions CONSTANT)
    Q_PROPERTY(bool canSubmit READ canSubmit NOTIFY canSubmitChanged)

public:
    explicit SubscriptionEditorViewModel(QObject *parent = nullptr);

    bool editMode() const;
    QString editTopic() const;
    QString topic() const;
    QString alias() const;
    int qos() const;
    int format() const;
    QString processorId() const;
    int processorIndex() const;
    QStringList processorOptionIds() const;
    QStringList processorOptionNames() const;
    QString processorBindingDetail() const;
    QString color() const;
    QStringList colorOptions() const;
    bool canSubmit() const;

    void setTopic(const QString &topic);
    void setAlias(const QString &alias);
    void setQos(int qos);
    void setFormat(int format);
    void setProcessorId(const QString &processorId);
    void setProcessorIndex(int index);
    void setColor(const QString &color);

    void openForCreate();
    void openForEdit(const SubscriptionEntry &subscription);
    void setProcessorOptions(const QVariantList &processors);
    SubscriptionEditorSubmission submission() const;

signals:
    void editModeChanged();
    void editTopicChanged();
    void topicChanged();
    void aliasChanged();
    void qosChanged();
    void formatChanged();
    void processorIdChanged();
    void processorIndexChanged();
    void processorOptionsChanged();
    void processorBindingDetailChanged();
    void colorChanged();
    void canSubmitChanged();

private:
    void setEditMode(bool editMode);
    void setEditTopic(const QString &topic);
    void rebuildProcessorOptions();
    void updateProcessorIndex();
    void updateProcessorBindingDetail();
    QVariantMap selectedProcessorRow() const;

    bool m_editMode = false;
    QString m_editTopic;
    QString m_topic;
    QString m_alias;
    int m_qos = 0;
    int m_format = 0;
    QString m_processorId;
    int m_processorIndex = 0;
    QStringList m_processorOptionIds {QString()};
    QStringList m_processorOptionNames;
    QString m_processorBindingDetail;
    QCborMap m_processorParameters;
    QVariantList m_processorRows;
    QString m_color;
};
