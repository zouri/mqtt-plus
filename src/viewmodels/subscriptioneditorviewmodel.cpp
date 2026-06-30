#include "viewmodels/subscriptioneditorviewmodel.h"

#include <algorithm>

SubscriptionEditorViewModel::SubscriptionEditorViewModel(QObject *parent)
    : QObject(parent)
{
}

bool SubscriptionEditorViewModel::editMode() const { return m_editMode; }
QString SubscriptionEditorViewModel::editTopic() const { return m_editTopic; }
QString SubscriptionEditorViewModel::topic() const { return m_topic; }
QString SubscriptionEditorViewModel::alias() const { return m_alias; }
int SubscriptionEditorViewModel::qos() const { return m_qos; }
int SubscriptionEditorViewModel::format() const { return m_format; }
QString SubscriptionEditorViewModel::scriptId() const { return m_scriptId; }
int SubscriptionEditorViewModel::scriptIndex() const { return m_scriptIndex; }
QStringList SubscriptionEditorViewModel::scriptOptionIds() const { return m_scriptOptionIds; }
QStringList SubscriptionEditorViewModel::scriptOptionNames() const { return m_scriptOptionNames; }
bool SubscriptionEditorViewModel::canSubmit() const { return !m_topic.trimmed().isEmpty(); }

void SubscriptionEditorViewModel::setTopic(const QString &topic)
{
    if (m_topic == topic) {
        return;
    }
    const bool wasSubmittable = canSubmit();
    m_topic = topic;
    emit topicChanged();
    if (wasSubmittable != canSubmit()) {
        emit canSubmitChanged();
    }
}

void SubscriptionEditorViewModel::setAlias(const QString &alias)
{
    if (m_alias == alias) {
        return;
    }
    m_alias = alias;
    emit aliasChanged();
}

void SubscriptionEditorViewModel::setQos(int qos)
{
    const int normalized = std::clamp(qos, 0, 1);
    if (m_qos == normalized) {
        return;
    }
    m_qos = normalized;
    emit qosChanged();
}

void SubscriptionEditorViewModel::setFormat(int format)
{
    const int normalized = std::max(0, format);
    if (m_format == normalized) {
        return;
    }
    m_format = normalized;
    emit formatChanged();
}

void SubscriptionEditorViewModel::setScriptId(const QString &scriptId)
{
    if (m_scriptId == scriptId) {
        return;
    }
    m_scriptId = scriptId;
    emit scriptIdChanged();
    updateScriptIndex();
}

void SubscriptionEditorViewModel::setScriptIndex(int index)
{
    const int lastIndex = std::max(0, static_cast<int>(m_scriptOptionIds.size()) - 1);
    const int normalized = std::clamp(index, 0, lastIndex);
    if (m_scriptIndex == normalized) {
        return;
    }
    m_scriptIndex = normalized;
    emit scriptIndexChanged();
    setScriptId(m_scriptOptionIds.value(normalized));
}

void SubscriptionEditorViewModel::openForCreate()
{
    setEditMode(false);
    setEditTopic(QString());
    setTopic(QString());
    setAlias(QString());
    setQos(0);
    setFormat(0);
    setScriptId(QString());
}

void SubscriptionEditorViewModel::openForEdit(const QVariantMap &subscription)
{
    setEditMode(true);
    setEditTopic(subscription.value(QStringLiteral("topic")).toString());
    setTopic(m_editTopic);
    setAlias(subscription.value(QStringLiteral("alias")).toString());
    setQos(subscription.value(QStringLiteral("requestedQos")).toInt());
    setFormat(subscription.value(QStringLiteral("format")).toInt());
    setScriptId(subscription.value(QStringLiteral("scriptId")).toString());
}

void SubscriptionEditorViewModel::setScriptOptions(const QVariantList &scripts)
{
    QStringList ids {QString()};
    QStringList names {QStringLiteral("None")};
    for (const QVariant &scriptValue : scripts) {
        const QVariantMap script = scriptValue.toMap();
        const QString id = script.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }
        ids.append(id);
        names.append(script.value(QStringLiteral("name")).toString());
    }

    if (m_scriptOptionIds == ids && m_scriptOptionNames == names) {
        updateScriptIndex();
        return;
    }

    m_scriptOptionIds = ids;
    m_scriptOptionNames = names;
    emit scriptOptionsChanged();
    updateScriptIndex();
}

QVariantMap SubscriptionEditorViewModel::submission() const
{
    QVariantMap result;
    result.insert(QStringLiteral("editMode"), m_editMode);
    result.insert(QStringLiteral("editTopic"), m_editTopic);
    result.insert(QStringLiteral("topic"), m_topic.trimmed());
    result.insert(QStringLiteral("alias"), m_alias);
    result.insert(QStringLiteral("qos"), m_qos);
    result.insert(QStringLiteral("format"), m_format);
    result.insert(QStringLiteral("scriptId"), m_scriptId);
    return result;
}

void SubscriptionEditorViewModel::setEditMode(bool editMode)
{
    if (m_editMode == editMode) {
        return;
    }
    m_editMode = editMode;
    emit editModeChanged();
}

void SubscriptionEditorViewModel::setEditTopic(const QString &topic)
{
    if (m_editTopic == topic) {
        return;
    }
    m_editTopic = topic;
    emit editTopicChanged();
}

void SubscriptionEditorViewModel::updateScriptIndex()
{
    const int index = std::max(0, static_cast<int>(m_scriptOptionIds.indexOf(m_scriptId)));
    if (m_scriptIndex == index) {
        return;
    }
    m_scriptIndex = index;
    emit scriptIndexChanged();
}
