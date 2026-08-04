#pragma once

#include "domain/publishdraft.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

class DraftEditorViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentDraftId READ currentDraftId NOTIFY editorStateChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    Q_PROPERTY(QString defaultTopic READ defaultTopic WRITE setDefaultTopic NOTIFY defaultTopicChanged)
    Q_PROPERTY(QString payload READ payload WRITE setPayload NOTIFY payloadChanged)
    Q_PROPERTY(int format READ format WRITE setFormat NOTIFY formatChanged)
    Q_PROPERTY(int qos READ qos WRITE setQos NOTIFY qosChanged)
    Q_PROPERTY(bool retain READ retain WRITE setRetain NOTIFY retainChanged)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY editorStateChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY editorStateChanged)
    Q_PROPERTY(QString validationError READ validationError NOTIFY validationErrorChanged)

public:
    explicit DraftEditorViewModel(QObject *parent = nullptr);
    QString currentDraftId() const;
    QString name() const;
    QString description() const;
    QString defaultTopic() const;
    QString payload() const;
    int format() const;
    int qos() const;
    bool retain() const;
    bool hasUnsavedChanges() const;
    bool canSave() const;
    QString validationError() const;

    void setName(const QString &value);
    void setDescription(const QString &value);
    void setDefaultTopic(const QString &value);
    void setPayload(const QString &value);
    void setFormat(int value);
    void setQos(int value);
    void setRetain(bool value);
    void setValidationError(const QString &value);
    void loadDraft(const QVariantMap &row);
    void newDraft();
    void duplicateDraft(const QVariantMap &row, const QString &copyName);
    PublishDraft draft() const;

signals:
    void nameChanged();
    void descriptionChanged();
    void defaultTopicChanged();
    void payloadChanged();
    void formatChanged();
    void qosChanged();
    void retainChanged();
    void editorStateChanged();
    void validationErrorChanged();

private:
    void captureSavedState();

    QString m_currentDraftId;
    QString m_name;
    QString m_description;
    QString m_defaultTopic;
    QString m_payload;
    int m_format = 1;
    int m_qos = 0;
    bool m_retain = false;
    QString m_validationError;
    QString m_savedName;
    QString m_savedDescription;
    QString m_savedDefaultTopic;
    QString m_savedPayload;
    int m_savedFormat = 1;
    int m_savedQos = 0;
    bool m_savedRetain = false;
};
