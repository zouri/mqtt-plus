#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

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

    Q_INVOKABLE void clearCurrentLogs();
    Q_INVOKABLE int loadOlderCurrentSessionLogs();

signals:
    void logStreamChanged();
    void logStreamRowAppended(const QVariantMap &row);
    void logTextChanged();

private:
    EventHistoryService &m_history;
    EventStreamModel &m_logs;
};
