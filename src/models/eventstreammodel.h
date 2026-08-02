#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class EventStreamModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

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
    };
    Q_ENUM(Role)

    explicit EventStreamModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;

    void setRows(const QVariantList &rows);
    void appendRow(const QVariantMap &row);
    void appendRows(const QVariantList &rows);
    int appendRowsAndTrimFront(const QVariantList &rows, int limit);
    void prependRows(const QVariantList &rows);
    int prependRowsAndTrimBack(const QVariantList &rows, int limit);
    bool updateRowByHistoryId(qint64 historyId, const QVariantMap &row);
    void clear();
    void trimToLimit(int limit);
    bool lastRowEquals(const QVariantMap &row) const;

signals:
    void countChanged();

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

        bool operator==(const EventStreamRow &other) const = default;
    };

    static EventStreamRow rowFromMap(const QVariantMap &row);
    static QVector<EventStreamRow> rowsFromVariants(const QVariantList &rows);
    QVariant roleValue(const EventStreamRow &row, int role) const;

    QVector<EventStreamRow> m_rows;
};
