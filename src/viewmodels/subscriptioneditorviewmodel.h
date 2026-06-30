#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class SubscriptionEditorViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool editMode READ editMode NOTIFY editModeChanged)
    Q_PROPERTY(QString editTopic READ editTopic NOTIFY editTopicChanged)
    Q_PROPERTY(QString topic READ topic WRITE setTopic NOTIFY topicChanged)
    Q_PROPERTY(QString alias READ alias WRITE setAlias NOTIFY aliasChanged)
    Q_PROPERTY(int qos READ qos WRITE setQos NOTIFY qosChanged)
    Q_PROPERTY(int format READ format WRITE setFormat NOTIFY formatChanged)
    Q_PROPERTY(QString scriptId READ scriptId WRITE setScriptId NOTIFY scriptIdChanged)
    Q_PROPERTY(int scriptIndex READ scriptIndex WRITE setScriptIndex NOTIFY scriptIndexChanged)
    Q_PROPERTY(QStringList scriptOptionIds READ scriptOptionIds NOTIFY scriptOptionsChanged)
    Q_PROPERTY(QStringList scriptOptionNames READ scriptOptionNames NOTIFY scriptOptionsChanged)
    Q_PROPERTY(bool canSubmit READ canSubmit NOTIFY canSubmitChanged)

public:
    explicit SubscriptionEditorViewModel(QObject *parent = nullptr);

    bool editMode() const;
    QString editTopic() const;
    QString topic() const;
    QString alias() const;
    int qos() const;
    int format() const;
    QString scriptId() const;
    int scriptIndex() const;
    QStringList scriptOptionIds() const;
    QStringList scriptOptionNames() const;
    bool canSubmit() const;

    void setTopic(const QString &topic);
    void setAlias(const QString &alias);
    void setQos(int qos);
    void setFormat(int format);
    void setScriptId(const QString &scriptId);
    void setScriptIndex(int index);

    Q_INVOKABLE void openForCreate();
    Q_INVOKABLE void openForEdit(const QVariantMap &subscription);
    Q_INVOKABLE void setScriptOptions(const QVariantList &scripts);
    Q_INVOKABLE QVariantMap submission() const;

signals:
    void editModeChanged();
    void editTopicChanged();
    void topicChanged();
    void aliasChanged();
    void qosChanged();
    void formatChanged();
    void scriptIdChanged();
    void scriptIndexChanged();
    void scriptOptionsChanged();
    void canSubmitChanged();

private:
    void setEditMode(bool editMode);
    void setEditTopic(const QString &topic);
    void updateScriptIndex();

    bool m_editMode = false;
    QString m_editTopic;
    QString m_topic;
    QString m_alias;
    int m_qos = 0;
    int m_format = 0;
    QString m_scriptId;
    int m_scriptIndex = 0;
    QStringList m_scriptOptionIds {QString()};
    QStringList m_scriptOptionNames {QStringLiteral("None")};
};
