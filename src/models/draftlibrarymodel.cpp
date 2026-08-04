#include "models/draftlibrarymodel.h"

#include "services/payload/payloadcodec.h"

DraftLibraryModel::DraftLibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DraftLibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_drafts.size();
}

int DraftLibraryModel::count() const { return rowCount(); }

QVariant DraftLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_drafts.size()) {
        return {};
    }
    const PublishDraft &draft = m_drafts.at(index.row());
    bool formatOk = false;
    const PayloadFormat format = PayloadCodec::formatFromId(draft.formatId, &formatOk);
    switch (role) {
    case IdRole: return draft.id;
    case NameRole: return draft.name;
    case DescriptionRole: return draft.description;
    case DefaultTopicRole: return draft.defaultTopic;
    case PayloadRole: return draft.payload;
    case PayloadPreviewRole: return draft.payload.simplified().left(160);
    case FormatIdRole: return draft.formatId;
    case FormatRole: return static_cast<int>(formatOk ? format : PayloadFormat::Plaintext);
    case FormatNameRole: return PayloadCodec::formatName(formatOk ? format : PayloadFormat::Plaintext);
    case QosRole: return draft.qos;
    case RetainRole: return draft.retain;
    case CreatedAtRole: return draft.createdAt;
    case UpdatedAtRole: return draft.updatedAt;
    case LastUsedAtRole: return draft.lastUsedAt;
    default: return {};
    }
}

QHash<int, QByteArray> DraftLibraryModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {DefaultTopicRole, "defaultTopic"},
        {PayloadRole, "payload"},
        {PayloadPreviewRole, "payloadPreview"},
        {FormatIdRole, "formatId"},
        {FormatRole, "format"},
        {FormatNameRole, "formatName"},
        {QosRole, "qos"},
        {RetainRole, "retain"},
        {CreatedAtRole, "createdAt"},
        {UpdatedAtRole, "updatedAt"},
        {LastUsedAtRole, "lastUsedAt"},
    };
}

QVariantMap DraftLibraryModel::rowAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }
    QVariantMap result;
    const QModelIndex rowIndex = index(row, 0);
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        result.insert(QString::fromUtf8(it.value()), rowIndex.data(it.key()));
    }
    return result;
}

int DraftLibraryModel::indexOfId(const QString &id) const
{
    for (qsizetype index = 0; index < m_drafts.size(); ++index) {
        if (m_drafts.at(index).id == id) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

void DraftLibraryModel::setDrafts(const QVector<PublishDraft> &drafts)
{
    beginResetModel();
    m_drafts = drafts;
    endResetModel();
    emit countChanged();
}
