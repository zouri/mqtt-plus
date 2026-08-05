#include "viewmodels/subscriptioneditorviewmodel.h"

#include "domain/sessionconfig.h"

#include <QCoreApplication>

#include <algorithm>

namespace {

QString editorText(const char *source)
{
    return QCoreApplication::translate("SubscriptionEditorViewModel", source);
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
int SubscriptionEditorViewModel::processorRevisionMode() const { return m_processorRevisionMode; }
QString SubscriptionEditorViewModel::pinnedRevisionId() const { return m_pinnedRevisionId; }
int SubscriptionEditorViewModel::pinnedRevisionIndex() const { return m_pinnedRevisionIndex; }
QStringList SubscriptionEditorViewModel::pinnedRevisionOptionIds() const { return m_pinnedRevisionOptionIds; }
QStringList SubscriptionEditorViewModel::pinnedRevisionOptionNames() const { return m_pinnedRevisionOptionNames; }
QString SubscriptionEditorViewModel::processorBindingDetail() const { return m_processorBindingDetail; }
QString SubscriptionEditorViewModel::color() const { return m_color; }

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
    return !m_topic.trimmed().isEmpty()
        && (m_processorId.isEmpty()
            || m_processorRevisionMode == 0
            || !m_pinnedRevisionId.isEmpty());
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
    const bool wasSubmittable = canSubmit();
    m_processorId = normalized;
    emit processorIdChanged();
    rebuildProcessorOptions();
    rebuildPinnedRevisionOptions();
    updateProcessorBindingDetail();
    if (wasSubmittable != canSubmit()) {
        emit canSubmitChanged();
    }
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

void SubscriptionEditorViewModel::setProcessorRevisionMode(int mode)
{
    const int normalized = mode == 1 ? 1 : 0;
    if (m_processorRevisionMode == normalized) {
        return;
    }
    const bool wasSubmittable = canSubmit();
    m_processorRevisionMode = normalized;
    emit processorRevisionModeChanged();
    rebuildPinnedRevisionOptions();
    if (m_processorRevisionMode == 1
        && m_pinnedRevisionId.isEmpty()
        && !m_pinnedRevisionOptionIds.isEmpty()) {
        setPinnedRevisionId(m_pinnedRevisionOptionIds.first());
    }
    updateProcessorBindingDetail();
    if (wasSubmittable != canSubmit()) {
        emit canSubmitChanged();
    }
}

void SubscriptionEditorViewModel::setPinnedRevisionId(const QString &revisionId)
{
    const QString normalized = revisionId.trimmed();
    if (m_pinnedRevisionId == normalized) {
        return;
    }
    const bool wasSubmittable = canSubmit();
    m_pinnedRevisionId = normalized;
    emit pinnedRevisionIdChanged();
    rebuildPinnedRevisionOptions();
    updateProcessorBindingDetail();
    if (wasSubmittable != canSubmit()) {
        emit canSubmitChanged();
    }
}

void SubscriptionEditorViewModel::setPinnedRevisionIndex(int index)
{
    if (m_pinnedRevisionOptionIds.isEmpty()) {
        setPinnedRevisionId({});
        return;
    }
    const int normalized = std::clamp(
        index,
        0,
        static_cast<int>(m_pinnedRevisionOptionIds.size()) - 1);
    if (m_pinnedRevisionIndex != normalized) {
        m_pinnedRevisionIndex = normalized;
        emit pinnedRevisionIndexChanged();
    }
    setPinnedRevisionId(m_pinnedRevisionOptionIds.value(normalized));
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

void SubscriptionEditorViewModel::openForCreate()
{
    setEditMode(false);
    setEditTopic({});
    setTopic({});
    setAlias({});
    setQos(0);
    setFormat(0);
    m_processorParametersCborBase64.clear();
    setProcessorRevisionMode(0);
    setPinnedRevisionId({});
    setProcessorId({});
    setColor({});
}

void SubscriptionEditorViewModel::openForEdit(const QVariantMap &subscription)
{
    setEditMode(true);
    setEditTopic(subscription.value(QStringLiteral("topic")).toString());
    setTopic(m_editTopic);
    setAlias(subscription.value(QStringLiteral("alias")).toString());
    setQos(subscription.value(QStringLiteral("requestedQos")).toInt());
    setFormat(subscription.value(QStringLiteral("format")).toInt());
    m_processorParametersCborBase64 = subscription.value(
        QStringLiteral("processorParametersCborBase64")).toString();
    setProcessorId(subscription.value(QStringLiteral("processorId")).toString());
    setProcessorRevisionMode(
        subscription.value(QStringLiteral("processorRevisionMode")).toString()
                == QStringLiteral("pinned")
            ? 1
            : 0);
    setPinnedRevisionId(subscription.value(QStringLiteral("pinnedRevisionId")).toString());
    setColor(subscription.value(QStringLiteral("color")).toString());
}

void SubscriptionEditorViewModel::setProcessorOptions(const QVariantList &processors)
{
    m_processorRows = processors;
    rebuildProcessorOptions();
    rebuildPinnedRevisionOptions();
    updateProcessorBindingDetail();
}

QVariantMap SubscriptionEditorViewModel::submission() const
{
    return {
        {QStringLiteral("editMode"), m_editMode},
        {QStringLiteral("editTopic"), m_editTopic},
        {QStringLiteral("topic"), m_topic.trimmed()},
        {QStringLiteral("alias"), m_alias},
        {QStringLiteral("qos"), m_qos},
        {QStringLiteral("format"), m_format},
        {QStringLiteral("processorId"), m_processorId},
        {QStringLiteral("processorRevisionMode"), m_processorRevisionMode == 1
                ? QStringLiteral("pinned")
                : QStringLiteral("current")},
        {QStringLiteral("pinnedRevisionId"), m_processorRevisionMode == 1
                ? m_pinnedRevisionId
                : QString()},
        {QStringLiteral("processorParametersCborBase64"), m_processorParametersCborBase64},
        {QStringLiteral("color"), m_color},
    };
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
        bool hasReadyRevision = false;
        for (const QVariant &revisionValue : processor.value(
                 QStringLiteral("revisions")).toList()) {
            if (revisionValue.toMap().value(QStringLiteral("selectable")).toBool()) {
                hasReadyRevision = true;
                break;
            }
        }
        if (!processor.value(QStringLiteral("archived")).toBool()
            && (processor.value(QStringLiteral("readinessState")).toString()
                    == QStringLiteral("ready")
                || hasReadyRevision)) {
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

void SubscriptionEditorViewModel::rebuildPinnedRevisionOptions()
{
    QStringList ids;
    QStringList names;
    const QVariantMap processor = selectedProcessorRow();
    QString selectedName;
    for (const QVariant &revisionValue : processor.value(
             QStringLiteral("revisions")).toList()) {
        const QVariantMap revision = revisionValue.toMap();
        const QString id = revision.value(QStringLiteral("id")).toString();
        const QString name = editorText(QT_TRANSLATE_NOOP(
            "SubscriptionEditorViewModel",
            "Revision %1 · %2 · %3")).arg(
            revision.value(QStringLiteral("revisionNumber")).toLongLong())
                .arg(revision.value(QStringLiteral("languageName")).toString())
                .arg(revision.value(QStringLiteral("createdAt")).toString());
        if (id == m_pinnedRevisionId) {
            selectedName = name;
        }
        if (revision.value(QStringLiteral("selectable")).toBool()) {
            ids.append(id);
            names.append(name);
        }
    }
    if (!m_pinnedRevisionId.isEmpty() && !ids.contains(m_pinnedRevisionId)) {
        ids.append(m_pinnedRevisionId);
        names.append(editorText(QT_TRANSLATE_NOOP(
            "SubscriptionEditorViewModel",
            "Unavailable: %1")).arg(
            selectedName.isEmpty() ? m_pinnedRevisionId : selectedName));
    }
    const bool changed = ids != m_pinnedRevisionOptionIds
        || names != m_pinnedRevisionOptionNames;
    m_pinnedRevisionOptionIds = ids;
    m_pinnedRevisionOptionNames = names;
    if (changed) {
        emit pinnedRevisionOptionsChanged();
    }
    updatePinnedRevisionIndex();
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

void SubscriptionEditorViewModel::updatePinnedRevisionIndex()
{
    const int index = std::max(
        0,
        static_cast<int>(m_pinnedRevisionOptionIds.indexOf(m_pinnedRevisionId)));
    if (m_pinnedRevisionIndex == index) {
        return;
    }
    m_pinnedRevisionIndex = index;
    emit pinnedRevisionIndexChanged();
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
        } else if (processor.value(QStringLiteral("archived")).toBool()) {
            detail = editorText(QT_TRANSLATE_NOOP(
                "SubscriptionEditorViewModel",
                "The selected Message Processor is archived. The binding will be preserved."));
        } else if (m_processorRevisionMode == 0
                   && processor.value(QStringLiteral("readinessState")).toString()
                       != QStringLiteral("ready")) {
            detail = processor.value(QStringLiteral("readinessDetail")).toString();
            if (detail.isEmpty()) {
                detail = editorText(QT_TRANSLATE_NOOP(
                    "SubscriptionEditorViewModel",
                    "The current Processor Revision is unavailable."));
            }
        } else if (m_processorRevisionMode == 1) {
            bool foundReadyRevision = false;
            for (const QVariant &revisionValue : processor.value(
                     QStringLiteral("revisions")).toList()) {
                const QVariantMap revision = revisionValue.toMap();
                if (revision.value(QStringLiteral("id")).toString() == m_pinnedRevisionId) {
                    foundReadyRevision = revision.value(QStringLiteral("selectable")).toBool();
                    if (!foundReadyRevision) {
                        detail = revision.value(QStringLiteral("readinessDetail")).toString();
                    }
                    break;
                }
            }
            if (!foundReadyRevision && detail.isEmpty()) {
                detail = editorText(QT_TRANSLATE_NOOP(
                    "SubscriptionEditorViewModel",
                    "The pinned Processor Revision is unavailable. The binding will be preserved."));
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
