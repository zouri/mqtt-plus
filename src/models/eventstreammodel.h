#pragma once

#include "presentation/eventrow.h"

#include <QAbstractListModel>
#include <QVariantMap>
#include <QVector>

class EventStreamModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int messageCount READ messageCount NOTIFY messageCountChanged)

public:
    enum Role : int {
        KindRole = Qt::UserRole + 1,
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

    void setRows(const QVector<EventRow> &rows);
    void appendRow(const EventRow &row);
    int appendRowsAndTrimFront(const QVector<EventRow> &rows, int limit);
    int prependRowsAndTrimBack(const QVector<EventRow> &rows, int limit);
    bool updateRowByHistoryId(qint64 historyId, const EventRow &row);
    bool beginExpandedPayloadLoad(qint64 historyId);
    bool finishExpandedPayloadLoad(
        qint64 historyId,
        const QString &payload,
        const QString &state);
    void clear();
    void trimToLimit(int limit);
    bool lastRowEquals(const EventRow &row) const;

signals:
    void countChanged();
    void messageCountChanged();

private:
    static int messageCountInRange(
        const QVector<EventRow> &rows,
        int first,
        int count);
    QVariant roleValue(const EventRow &row, int role) const;

    QVector<EventRow> m_rows;
    int m_messageCount = 0;
};
