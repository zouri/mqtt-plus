#include "viewmodels/subscriptioneditorviewmodel.h"

#include "domain/sessionconfig.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace {

QString editorText(const char *source)
{
    return QCoreApplication::translate("SubscriptionEditorViewModel", source);
}

QStringList topicFiltersFromEditorText(const QString &text)
{
    QStringList filters;
    QSet<QString> seen;
    const QStringList candidates = text.split(
        QRegularExpression(QStringLiteral("[,\\r\\n]+")),
        Qt::SkipEmptyParts);
    for (const QString &candidate : candidates) {
        const QString filter = candidate.trimmed();
        if (!filter.isEmpty() && !seen.contains(filter)) {
            filters.append(filter);
            seen.insert(filter);
        }
    }
    return filters;
}

} // namespace

SubscriptionEditorViewModel::SubscriptionEditorViewModel(QObject *parent)
    : QObject(parent)
    , m_processorOptionNames {
          editorText(QT_TRANSLATE_NOOP("SubscriptionEditorViewModel", "None")),
      }
{
}

bool SubscriptionEditorViewModel::editMode() const { return m_editMode; }
QString SubscriptionEditorViewModel::editTopic() const { return m_editTopic; }
QString SubscriptionEditorViewModel::topic() const { return m_topic; }
QString SubscriptionEditorViewModel::alias() const { return m_alias; }
int SubscriptionEditorViewModel::qos() const { return m_qos; }
int SubscriptionEditorViewModel::format() const { return m_format; }
QString SubscriptionEditorViewModel::processorId() const { return m_processorId; }
int SubscriptionEditorViewModel::processorIndex() const { return m_processorIndex; }
QStringList SubscriptionEditorViewModel::processorOptionIds() const { return m_processorOptionIds; }
QStringList SubscriptionEditorViewModel::processorOptionNames() const { return m_processorOptionNames; }
QString SubscriptionEditorViewModel::processorBindingDetail() const { return m_processorBindingDetail; }
QString SubscriptionEditorViewModel::color() const { return m_color; }
bool SubscriptionEditorViewModel::noLocal() const { return m_noLocal; }
QString SubscriptionEditorViewModel::subscriptionIdentifierText() const { return m_subscriptionIdentifierText; }
QString SubscriptionEditorViewModel::userPropertiesText() const { return m_userPropertiesText; }

QStringList SubscriptionEditorViewModel::colorOptions() const
{
    return {
        QString(),
        QStringLiteral("#0071E3"),
        QStringLiteral("#34C759"),
        QStringLiteral("#FF9500"),
        QStringLiteral("#AF52DE"),
        QStringLiteral("#FF2D55"),
        QStringLiteral("#5AC8FA"),
        QStringLiteral("#5856D6"),
        QStringLiteral("#8E8E93"),
    };
}

bool SubscriptionEditorViewModel::canSubmit() const
{
    bool identifierOk = false;
    const QString identifierText = m_subscriptionIdentifierText.trimmed();
    const qulonglong identifier = identifierText.toULongLong(&identifierOk);
    const bool hasValidIdentifier = identifierText.isEmpty()
        || (identifierOk
            && identifier > 0
            && identifier <= SessionConfig::kMaximumSubscriptionIdentifier);
    return hasValidIdentifier && (m_editMode
        ? !m_topic.trimmed().isEmpty()
        : !topicFiltersFromEditorText(m_topic).isEmpty());
}

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
    const int normalized = SessionConfig::sanitizeQos(qos);
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

void SubscriptionEditorViewModel::setProcessorId(const QString &processorId)
{
    const QString normalized = processorId.trimmed();
    if (m_processorId == normalized) {
        return;
    }
    m_processorId = normalized;
    emit processorIdChanged();
    rebuildProcessorOptions();
    updateProcessorBindingDetail();
}

void SubscriptionEditorViewModel::setProcessorIndex(int index)
{
    const int lastIndex = std::max(0, static_cast<int>(m_processorOptionIds.size()) - 1);
    const int normalized = std::clamp(index, 0, lastIndex);
    if (m_processorIndex != normalized) {
        m_processorIndex = normalized;
        emit processorIndexChanged();
    }
    setProcessorId(m_processorOptionIds.value(normalized));
}

void SubscriptionEditorViewModel::setColor(const QString &color)
{
    const QString normalized = color.trimmed();
    if (m_color == normalized) {
        return;
    }
    m_color = normalized;
    emit colorChanged();
}

void SubscriptionEditorViewModel::setNoLocal(bool noLocal)
{
    if (m_noLocal == noLocal) {
        return;
    }
    m_noLocal = noLocal;
    emit optionsChanged();
}

void SubscriptionEditorViewModel::setSubscriptionIdentifierText(const QString &text)
{
    if (m_subscriptionIdentifierText == text) {
        return;
    }
    const bool wasSubmittable = canSubmit();
    m_subscriptionIdentifierText = text;
    emit optionsChanged();
    if (wasSubmittable != canSubmit()) {
        emit canSubmitChanged();
    }
}

void SubscriptionEditorViewModel::setUserPropertiesText(const QString &text)
{
    if (m_userPropertiesText == text) {
        return;
    }
    m_userPropertiesText = text;
    emit optionsChanged();
}

void SubscriptionEditorViewModel::openForCreate()
{
    setEditMode(false);
    setEditTopic({});
    setTopic({});
    setAlias({});
    setQos(0);
    setFormat(0);
    m_processorParameters.clear();
    setProcessorId({});
    setColor({});
    setNoLocal(false);
    setSubscriptionIdentifierText({});
    setUserPropertiesText({});
}

void SubscriptionEditorViewModel::openForEdit(const SubscriptionEntry &subscription)
{
    setEditMode(true);
    setEditTopic(subscription.topic);
    setTopic(m_editTopic);
    setAlias(subscription.alias);
    setQos(subscription.requestedQos);
    setFormat(subscription.format);
    m_processorParameters = subscription.processor.parameters;
    setProcessorId(subscription.processor.processorId);
    setColor(subscription.color);
    setNoLocal(subscription.options.noLocal);
    setSubscriptionIdentifierText(subscription.options.subscriptionIdentifier > 0
            ? QString::number(subscription.options.subscriptionIdentifier)
            : QString());
    setUserPropertiesText(mqttUserPropertiesToText(subscription.options.userProperties));
}

void SubscriptionEditorViewModel::setProcessorOptions(const QVariantList &processors)
{
    m_processorRows = processors;
    rebuildProcessorOptions();
    updateProcessorBindingDetail();
}

SubscriptionEditorSubmission SubscriptionEditorViewModel::submission() const
{
    const QString normalizedTopic = m_topic.trimmed();
    bool identifierOk = false;
    const qulonglong identifier = m_subscriptionIdentifierText.trimmed().toULongLong(&identifierOk);
    return SubscriptionEditorSubmission {
        .editMode = m_editMode,
        .editTopic = m_editTopic,
        .topic = normalizedTopic,
        .topics = m_editMode
            ? QStringList {normalizedTopic}
            : topicFiltersFromEditorText(m_topic),
        .alias = m_alias,
        .qos = m_qos,
        .format = m_format,
        .processor = {
            .processorId = m_processorId,
            .parameters = m_processorParameters,
        },
        .color = m_color,
        .options = {
            .noLocal = m_noLocal,
            .subscriptionIdentifier = identifierOk
                    && identifier <= SessionConfig::kMaximumSubscriptionIdentifier
                ? static_cast<quint32>(identifier)
                : 0,
            .userProperties = mqttUserPropertiesFromText(m_userPropertiesText),
        },
    };
}

void SubscriptionEditorViewModel::setEditMode(bool editMode)
{
    if (m_editMode == editMode) {
        return;
    }
    const bool wasSubmittable = canSubmit();
    m_editMode = editMode;
    emit editModeChanged();
    if (wasSubmittable != canSubmit()) {
        emit canSubmitChanged();
    }
}

void SubscriptionEditorViewModel::setEditTopic(const QString &topic)
{
    if (m_editTopic == topic) {
        return;
    }
    m_editTopic = topic;
    emit editTopicChanged();
}

void SubscriptionEditorViewModel::rebuildProcessorOptions()
{
    QStringList ids {QString()};
    QStringList names {
        editorText(QT_TRANSLATE_NOOP("SubscriptionEditorViewModel", "None")),
    };
    QString selectedName;
    for (const QVariant &processorValue : m_processorRows) {
        const QVariantMap processor = processorValue.toMap();
        const QString id = processor.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }
        const QString name = processor.value(QStringLiteral("name")).toString();
        if (id == m_processorId) {
            selectedName = name;
        }
        if (processor.value(QStringLiteral("readinessState")).toString()
                == QStringLiteral("ready")) {
            ids.append(id);
            names.append(name);
        }
    }
    if (!m_processorId.isEmpty() && !ids.contains(m_processorId)) {
        ids.append(m_processorId);
        names.append(editorText(QT_TRANSLATE_NOOP(
            "SubscriptionEditorViewModel",
            "Unavailable: %1")).arg(
            selectedName.isEmpty() ? m_processorId : selectedName));
    }
    const bool changed = ids != m_processorOptionIds || names != m_processorOptionNames;
    m_processorOptionIds = ids;
    m_processorOptionNames = names;
    if (changed) {
        emit processorOptionsChanged();
    }
    updateProcessorIndex();
}

void SubscriptionEditorViewModel::updateProcessorIndex()
{
    const int index = std::max(0, static_cast<int>(m_processorOptionIds.indexOf(m_processorId)));
    if (m_processorIndex == index) {
        return;
    }
    m_processorIndex = index;
    emit processorIndexChanged();
}

void SubscriptionEditorViewModel::updateProcessorBindingDetail()
{
    QString detail;
    if (!m_processorId.isEmpty()) {
        const QVariantMap processor = selectedProcessorRow();
        if (processor.isEmpty()) {
            detail = editorText(QT_TRANSLATE_NOOP(
                "SubscriptionEditorViewModel",
                "The selected Message Processor is unavailable. The binding will be preserved."));
        } else if (processor.value(QStringLiteral("readinessState")).toString()
                   != QStringLiteral("ready")) {
            detail = processor.value(QStringLiteral("readinessDetail")).toString();
            if (detail.isEmpty()) {
                detail = editorText(QT_TRANSLATE_NOOP(
                    "SubscriptionEditorViewModel",
                    "The selected Message Processor is unavailable."));
            }
        }
    }
    if (m_processorBindingDetail == detail) {
        return;
    }
    m_processorBindingDetail = detail;
    emit processorBindingDetailChanged();
}

QVariantMap SubscriptionEditorViewModel::selectedProcessorRow() const
{
    for (const QVariant &processorValue : m_processorRows) {
        const QVariantMap processor = processorValue.toMap();
        if (processor.value(QStringLiteral("id")).toString() == m_processorId) {
            return processor;
        }
    }
    return {};
}
