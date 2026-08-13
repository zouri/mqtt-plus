#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class EventStreamModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int messageCount READ messageCount NOTIFY messageCountChanged)

public:
    enum Role : int {
        IdRole = Qt::UserRole + 1,
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
        ParseStateRole,
        PayloadStateRole,
        PayloadHashRole,
        ExpandedPayloadRole,
        ExpandedPayloadStateRole,
        ExpandedPayloadNeededRole,
    };
    Q_ENUM(Role)

    explicit EventStreamModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    int messageCount() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;

    void setRows(const QVariantList &rows);
    void appendRow(const QVariantMap &row);
    int appendRowsAndTrimFront(const QVariantList &rows, int limit);
    int prependRowsAndTrimBack(const QVariantList &rows, int limit);
    bool updateRowByHistoryId(qint64 historyId, const QVariantMap &row);
    bool beginExpandedPayloadLoad(qint64 historyId);
    bool finishExpandedPayloadLoad(
        qint64 historyId,
        const QString &payload,
        const QString &state);
    void clear();
    void trimToLimit(int limit);
    bool lastRowEquals(const QVariantMap &row) const;

signals:
    void countChanged();
    void messageCountChanged();

private:
    struct EventStreamRow {
        QVariantMap source;
        QVariant id;
        QString kind;
        QString timestamp;
        QString title;
        QString payload;
        QString payloadFormat;
        int payloadSize = 0;
        QString topic;
        QString topicColor;
        QString testPayload;
        int testFormat = 0;
        QString testFormatName;
        qint64 historyId = 0;
        QString direction;
        QString alias;
        int qos = -1;
        bool retain = false;
        bool retainKnown = false;
        QString parsedPayload;
        QString parseState;
        QString payloadState;
        QString payloadHash;
        QString expandedPayload;
        QString expandedPayloadState;
        bool expandedPayloadNeeded = false;

        bool operator==(const EventStreamRow &other) const = default;
    };

    static EventStreamRow rowFromMap(const QVariantMap &row);
    static QVector<EventStreamRow> rowsFromVariants(const QVariantList &rows);
    static int messageCountInRange(
        const QVector<EventStreamRow> &rows,
        int first,
        int count);
    QVariant roleValue(const EventStreamRow &row, int role) const;

    QVector<EventStreamRow> m_rows;
    int m_messageCount = 0;
};
