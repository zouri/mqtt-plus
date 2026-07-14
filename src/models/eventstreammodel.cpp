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
    return roleValue(m_rows.at(index.row()), role);
}

QHash<int, QByteArray> EventStreamModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {IdRole, "id"},
        {KindRole, "kind"},
        {TimestampRole, "timestamp"},
        {TitleRole, "title"},
        {PayloadRole, "payload"},
        {PayloadFormatRole, "payloadFormat"},
        {PayloadSizeRole, "payloadSize"},
        {TopicRole, "topic"},
        {TopicColorRole, "topicColor"},
        {TestPayloadRole, "testPayload"},
        {TestFormatRole, "testFormat"},
        {TestFormatNameRole, "testFormatName"},
        {HistoryIdRole, "historyId"},
        {DirectionRole, "direction"},
        {AliasRole, "alias"},
        {QosRole, "qos"},
        {RetainRole, "retain"},
        {RetainKnownRole, "retainKnown"},
        {ParsedPayloadRole, "parsedPayload"},
        {PayloadStateRole, "payloadState"},
        {PayloadHashRole, "payloadHash"},
    };
    return roles;
}

QVariantMap EventStreamModel::rowAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }
    return m_rows.at(row).source;
}

void EventStreamModel::setRows(const QVariantList &rows)
{
    const QVector<EventStreamRow> convertedRows = rowsFromVariants(rows);
    if (m_rows == convertedRows) {
        return;
    }

    const bool countWillChange = convertedRows.size() != m_rows.size();
    if (!countWillChange) {
        m_rows = convertedRows;
        if (!m_rows.isEmpty()) {
            emit dataChanged(index(0, 0),
                             index(static_cast<int>(m_rows.size() - 1), 0),
                             {IdRole,
                              KindRole,
                              TimestampRole,
                              TitleRole,
                              PayloadRole,
                              PayloadFormatRole,
                              PayloadSizeRole,
                              TopicRole,
                              TopicColorRole,
                              TestPayloadRole,
                              TestFormatRole,
                              TestFormatNameRole,
                              HistoryIdRole,
                              DirectionRole,
                              AliasRole,
                              QosRole,
                              RetainRole,
                              RetainKnownRole,
                              ParsedPayloadRole,
                              PayloadStateRole,
                              PayloadHashRole});
        }
        return;
    }

    beginResetModel();
    m_rows = convertedRows;
    endResetModel();
    emit countChanged();
}

void EventStreamModel::appendRow(const QVariantMap &row)
{
    const int insertRow = m_rows.size();
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_rows.append(rowFromMap(row));
    endInsertRows();
    emit countChanged();
}

void EventStreamModel::appendRows(const QVariantList &rows)
{
    if (rows.isEmpty()) {
        return;
    }

    const int firstRow = m_rows.size();
    const int lastRow = firstRow + rows.size() - 1;
    beginInsertRows(QModelIndex(), firstRow, lastRow);
    for (const QVariant &row : rows) {
        m_rows.append(rowFromMap(row.toMap()));
    }
    endInsertRows();
    emit countChanged();
}

void EventStreamModel::prependRows(const QVariantList &rows)
{
    if (rows.isEmpty()) {
        return;
    }

    beginInsertRows(QModelIndex(), 0, rows.size() - 1);
    QVector<EventStreamRow> mergedRows = rowsFromVariants(rows);
    mergedRows.reserve(mergedRows.size() + m_rows.size());
    for (const EventStreamRow &row : std::as_const(m_rows)) {
        mergedRows.append(row);
    }
    m_rows = mergedRows;
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

EventStreamModel::EventStreamRow EventStreamModel::rowFromMap(const QVariantMap &row)
{
    EventStreamRow streamRow;
    streamRow.source = row;
    streamRow.id = row.value(QStringLiteral("id"));
    streamRow.kind = row.value(QStringLiteral("kind")).toString();
    streamRow.timestamp = row.value(QStringLiteral("timestamp")).toString();
    streamRow.title = row.value(QStringLiteral("title")).toString();
    streamRow.payload = row.value(QStringLiteral("payload")).toString();
    streamRow.payloadFormat = row.value(QStringLiteral("payloadFormat")).toString();
    streamRow.payloadSize = row.value(QStringLiteral("payloadSize")).toInt();
    streamRow.topic = row.value(QStringLiteral("topic")).toString();
    streamRow.topicColor = row.value(QStringLiteral("topicColor"), QString()).toString();
    streamRow.testPayload = row.value(QStringLiteral("testPayload"), QString()).toString();
    streamRow.testFormat = row.value(QStringLiteral("testFormat"), 0).toInt();
    streamRow.testFormatName = row.value(QStringLiteral("testFormatName"), QString()).toString();
    streamRow.historyId = row.value(QStringLiteral("historyId"), 0).toLongLong();
    streamRow.direction = row.value(QStringLiteral("direction")).toString();
    streamRow.alias = row.value(QStringLiteral("alias")).toString();
    streamRow.qos = row.value(QStringLiteral("qos"), -1).toInt();
    streamRow.retain = row.value(QStringLiteral("retain")).toBool();
    streamRow.retainKnown = row.value(QStringLiteral("retainKnown")).toBool();
    streamRow.parsedPayload = row.value(QStringLiteral("parsedPayload")).toString();
    streamRow.payloadState = row.value(QStringLiteral("payloadState")).toString();
    streamRow.payloadHash = row.value(QStringLiteral("payloadHash")).toString();
    return streamRow;
}

QVector<EventStreamModel::EventStreamRow> EventStreamModel::rowsFromVariants(const QVariantList &rows)
{
    QVector<EventStreamRow> convertedRows;
    convertedRows.reserve(rows.size());
    for (const QVariant &row : rows) {
        convertedRows.append(rowFromMap(row.toMap()));
    }
    return convertedRows;
}

QVariant EventStreamModel::roleValue(const EventStreamRow &row, int role) const
{
    switch (role) {
    case IdRole:
        return row.id;
    case KindRole:
        return row.kind;
    case TimestampRole:
        return row.timestamp;
    case TitleRole:
        return row.title;
    case PayloadRole:
        return row.payload;
    case PayloadFormatRole:
        return row.payloadFormat;
    case PayloadSizeRole:
        return row.payloadSize;
    case TopicRole:
        return row.topic;
    case TopicColorRole:
        return row.topicColor;
    case TestPayloadRole:
        return row.testPayload;
    case TestFormatRole:
        return row.testFormat;
    case TestFormatNameRole:
        return row.testFormatName;
    case HistoryIdRole:
        return QString::number(row.historyId);
    case DirectionRole:
        return row.direction;
    case AliasRole:
        return row.alias;
    case QosRole:
        return row.qos;
    case RetainRole:
        return row.retain;
    case RetainKnownRole:
        return row.retainKnown;
    case ParsedPayloadRole:
        return row.parsedPayload;
    case PayloadStateRole:
        return row.payloadState;
    case PayloadHashRole:
        return row.payloadHash;
    default:
        return {};
    }
}
