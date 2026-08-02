#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

class EventHistoryService;
class EventStreamModel;

class LogsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(EventStreamModel* logs READ logs CONSTANT)
    Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)

public:
    explicit LogsViewModel(
        EventHistoryService &history,
        EventStreamModel &logs,
        QObject *parent = nullptr);

    EventStreamModel *logs() const;
    QString logText() const;
    static QString formattedLogRow(const QVariantMap &row);
    static QString renderedLogText(const EventStreamModel *model);

signals:
    void logTextChanged();
    void logTextReset();
    void logTextInserted(int position, const QString &text);
    void logTextRemoved(int start, int end);

private:
    void rebuildCachedText();
    void handleRowsInserted(int first, int last);
    void handleRowsAboutToBeRemoved(int first, int last);
    int rowStartPosition(int row) const;

    EventHistoryService &m_history;
    EventStreamModel &m_logs;
    QString m_logText;
    QVector<int> m_rowTextLengths;
};
