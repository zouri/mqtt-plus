#include "eventstreammodel.h"

EventStreamModel::EventStreamModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int EventStreamModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int EventStreamModel::count() const
{
    return rowCount();
}

QVariant EventStreamModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    return roleValue(m_rows.at(index.row()).toMap(), role);
}

QHash<int, QByteArray> EventStreamModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {KindRole, "kind"},
        {TimestampRole, "timestamp"},
        {TitleRole, "title"},
        {PayloadRole, "payload"},
        {PayloadFormatRole, "payloadFormat"},
        {PayloadSizeRole, "payloadSize"},
        {TopicRole, "topic"},
        {TestPayloadRole, "testPayload"},
        {TestFormatRole, "testFormat"},
        {TestFormatNameRole, "testFormatName"},
    };
}

QVariantMap EventStreamModel::rowAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }
    return m_rows.at(row).toMap();
}

void EventStreamModel::setRows(const QVariantList &rows)
{
    const bool countWillChange = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = rows;
    endResetModel();
    if (countWillChange) {
        emit countChanged();
    }
}

void EventStreamModel::appendRow(const QVariantMap &row)
{
    const int insertRow = m_rows.size();
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_rows.append(row);
    endInsertRows();
    emit countChanged();
}

void EventStreamModel::prependRows(const QVariantList &rows)
{
    if (rows.isEmpty()) {
        return;
    }

    beginInsertRows(QModelIndex(), 0, rows.size() - 1);
    for (int i = rows.size() - 1; i >= 0; --i) {
        m_rows.prepend(rows.at(i));
    }
    endInsertRows();
    emit countChanged();
}

void EventStreamModel::clear()
{
    if (m_rows.isEmpty()) {
        return;
    }

    beginRemoveRows(QModelIndex(), 0, m_rows.size() - 1);
    m_rows.clear();
    endRemoveRows();
    emit countChanged();
}

void EventStreamModel::trimToLimit(int limit)
{
    const int overflow = m_rows.size() - limit;
    if (overflow <= 0) {
        return;
    }

    beginRemoveRows(QModelIndex(), 0, overflow - 1);
    m_rows.erase(m_rows.begin(), m_rows.begin() + overflow);
    endRemoveRows();
    emit countChanged();
}

QVariant EventStreamModel::roleValue(const QVariantMap &row, int role) const
{
    switch (role) {
    case IdRole:
        return row.value(QStringLiteral("id"));
    case KindRole:
        return row.value(QStringLiteral("kind"));
    case TimestampRole:
        return row.value(QStringLiteral("timestamp"));
    case TitleRole:
        return row.value(QStringLiteral("title"));
    case PayloadRole:
        return row.value(QStringLiteral("payload"));
    case PayloadFormatRole:
        return row.value(QStringLiteral("payloadFormat"));
    case PayloadSizeRole:
        return row.value(QStringLiteral("payloadSize"));
    case TopicRole:
        return row.value(QStringLiteral("topic"));
    case TestPayloadRole:
        return row.value(QStringLiteral("testPayload"));
    case TestFormatRole:
        return row.value(QStringLiteral("testFormat"));
    case TestFormatNameRole:
        return row.value(QStringLiteral("testFormatName"));
    default:
        return {};
    }
}
