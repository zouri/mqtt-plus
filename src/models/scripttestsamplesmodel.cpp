#include "scripttestsamplesmodel.h"

ScriptTestSamplesModel::ScriptTestSamplesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ScriptTestSamplesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_sampleRows.size();
}

int ScriptTestSamplesModel::count() const
{
    return rowCount();
}

QVariant ScriptTestSamplesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_sampleRows.size()) {
        return {};
    }

    const QVariantMap &row = m_sampleRows.at(index.row()).toMap();
    switch (role) {
    case TopicRole:
        return row.value(QStringLiteral("topic"));
    case PayloadRole:
        return row.value(QStringLiteral("testPayload"));
    case FormatRole:
        return row.value(QStringLiteral("testFormat"));
    case FormatNameRole:
        return row.value(QStringLiteral("testFormatName"));
    case TimestampRole:
        return row.value(QStringLiteral("timestamp"));
    case PayloadSizeRole:
        return row.value(QStringLiteral("payloadSize"));
    default:
        return {};
    }
}

QHash<int, QByteArray> ScriptTestSamplesModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {TopicRole, "topic"},
        {PayloadRole, "payload"},
        {FormatRole, "format"},
        {FormatNameRole, "formatName"},
        {TimestampRole, "timestamp"},
        {PayloadSizeRole, "payloadSize"},
    };
    return roles;
}

QVariantMap ScriptTestSamplesModel::rowAt(int row) const
{
    if (row < 0 || row >= m_sampleRows.size()) {
        return {};
    }
    return m_sampleRows.at(row).toMap();
}

void ScriptTestSamplesModel::setSource(const QVariantList *messageRows)
{
    m_messageRows = messageRows;
    rebuild(0);
}

void ScriptTestSamplesModel::notifyRefresh()
{
    rebuild(0);
}

void ScriptTestSamplesModel::rebuild(int newCount)
{
    Q_UNUSED(newCount)

    QVariantList samples;
    if (m_messageRows) {
        constexpr int kMaxScriptTestSamples = 24;
        samples.reserve(kMaxScriptTestSamples);
        for (auto it = m_messageRows->crbegin();
             it != m_messageRows->crend() && samples.size() < kMaxScriptTestSamples;
             ++it) {
            samples.append(*it);
        }
    }

    const bool countWillChange = samples.size() != m_sampleRows.size();
    if (samples == m_sampleRows) {
        return;
    }

    if (!countWillChange) {
        m_sampleRows = std::move(samples);
        if (!m_sampleRows.isEmpty()) {
            emit dataChanged(
                index(0, 0),
                index(m_sampleRows.size() - 1, 0),
                {
                    TopicRole,
                    PayloadRole,
                    FormatRole,
                    FormatNameRole,
                    TimestampRole,
                    PayloadSizeRole,
                });
        }
        return;
    }

    beginResetModel();
    m_sampleRows = std::move(samples);
    endResetModel();
    emit countChanged();
}
