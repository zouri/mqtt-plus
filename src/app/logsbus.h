#pragma once

#include <QObject>
#include <QVariantMap>

class EventController;
class EventStreamModel;

class LogsBus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(EventStreamModel* logs READ logs CONSTANT)

public:
    struct Dependencies
    {
        EventStreamModel *logsModel = nullptr;
        EventController *eventController = nullptr;
    };

    explicit LogsBus(Dependencies dependencies, QObject *parent = nullptr);

    EventStreamModel *logs();

    Q_INVOKABLE int loadOlderCurrentSessionLogs();
    Q_INVOKABLE void clearCurrentLogs();

signals:
    void logsChanged();
    void logsRowAppended(const QVariantMap &row);

private:
    Dependencies m_dependencies;
};
