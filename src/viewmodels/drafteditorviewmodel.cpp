#include "viewmodels/drafteditorviewmodel.h"

#include "domain/sessionconfig.h"
#include "services/payload/payloadcodec.h"

#include <algorithm>

DraftEditorViewModel::DraftEditorViewModel(QObject *parent)
    : QObject(parent)
{
}

QString DraftEditorViewModel::currentDraftId() const { return m_currentDraftId; }
QString DraftEditorViewModel::name() const { return m_name; }
QString DraftEditorViewModel::description() const { return m_description; }
QString DraftEditorViewModel::defaultTopic() const { return m_defaultTopic; }
QString DraftEditorViewModel::payload() const { return m_payload; }
int DraftEditorViewModel::format() const { return m_format; }
int DraftEditorViewModel::qos() const { return m_qos; }
bool DraftEditorViewModel::retain() const { return m_retain; }
QString DraftEditorViewModel::validationError() const { return m_validationError; }

bool DraftEditorViewModel::hasUnsavedChanges() const
{
    return m_name != m_savedName
        || m_description != m_savedDescription
        || m_defaultTopic != m_savedDefaultTopic
        || m_payload != m_savedPayload
        || m_format != m_savedFormat
        || m_qos != m_savedQos
        || m_retain != m_savedRetain;
}

bool DraftEditorViewModel::canSave() const
{
    return !m_name.trimmed().isEmpty() && (m_currentDraftId.isEmpty() || hasUnsavedChanges());
}

void DraftEditorViewModel::setName(const QString &value)
{
    if (m_name == value) return;
    m_name = value;
    emit nameChanged();
    emit editorStateChanged();
}

void DraftEditorViewModel::setDescription(const QString &value)
{
    if (m_description == value) return;
    m_description = value;
    emit descriptionChanged();
    emit editorStateChanged();
}

void DraftEditorViewModel::setDefaultTopic(const QString &value)
{
    if (m_defaultTopic == value) return;
    m_defaultTopic = value;
    emit defaultTopicChanged();
    emit editorStateChanged();
}

void DraftEditorViewModel::setPayload(const QString &value)
{
    if (m_payload == value) return;
    m_payload = value;
    emit payloadChanged();
    emit editorStateChanged();
}

void DraftEditorViewModel::setFormat(int value)
{
    const int normalized = std::clamp(value, 0, 5);
    if (m_format == normalized) return;
    m_format = normalized;
    emit formatChanged();
    emit editorStateChanged();
}

void DraftEditorViewModel::setQos(int value)
{
    const int normalized = SessionConfig::sanitizeQos(value);
    if (m_qos == normalized) return;
    m_qos = normalized;
    emit qosChanged();
    emit editorStateChanged();
}

void DraftEditorViewModel::setRetain(bool value)
{
    if (m_retain == value) return;
    m_retain = value;
    emit retainChanged();
    emit editorStateChanged();
}

void DraftEditorViewModel::setValidationError(const QString &value)
{
    if (m_validationError == value) return;
    m_validationError = value;
    emit validationErrorChanged();
}

void DraftEditorViewModel::loadDraft(const QVariantMap &row)
{
    m_currentDraftId = row.value(QStringLiteral("id")).toString();
    setName(row.value(QStringLiteral("name")).toString());
    setDescription(row.value(QStringLiteral("description")).toString());
    setDefaultTopic(row.value(QStringLiteral("defaultTopic")).toString());
    setPayload(row.value(QStringLiteral("payload")).toString());
    setFormat(row.value(QStringLiteral("format"), 1).toInt());
    setQos(row.value(QStringLiteral("qos"), 0).toInt());
    setRetain(row.value(QStringLiteral("retain"), false).toBool());
    captureSavedState();
    setValidationError(QString());
    emit editorStateChanged();
}

void DraftEditorViewModel::newDraft()
{
    m_currentDraftId.clear();
    setName(QString());
    setDescription(QString());
    setDefaultTopic(QString());
    setPayload(QString());
    setFormat(1);
    setQos(0);
    setRetain(false);
    captureSavedState();
    setValidationError(QString());
    emit editorStateChanged();
}

void DraftEditorViewModel::duplicateDraft(const QVariantMap &row, const QString &copyName)
{
    loadDraft(row);
    m_currentDraftId.clear();
    setName(copyName);
    m_savedName.clear();
    m_savedDescription.clear();
    m_savedDefaultTopic.clear();
    m_savedPayload.clear();
    m_savedFormat = 1;
    m_savedQos = 0;
    m_savedRetain = false;
    emit editorStateChanged();
}

PublishDraft DraftEditorViewModel::draft() const
{
    PublishDraft result;
    result.id = m_currentDraftId;
    result.name = m_name;
    result.description = m_description;
    result.defaultTopic = m_defaultTopic;
    result.payload = m_payload;
    result.formatId = PayloadCodec::formatId(PayloadCodec::formatFromInt(m_format));
    result.qos = m_qos;
    result.retain = m_retain;
    return result;
}

void DraftEditorViewModel::captureSavedState()
{
    m_savedName = m_name;
    m_savedDescription = m_description;
    m_savedDefaultTopic = m_defaultTopic;
    m_savedPayload = m_payload;
    m_savedFormat = m_format;
    m_savedQos = m_qos;
    m_savedRetain = m_retain;
}
