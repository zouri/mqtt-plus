#pragma once

#include <QObject>
#include <QVariantMap>

class EventController;
class EventStreamModel;

class LogStreamFacade : public QObject
{
    Q_OBJECT
    Q_PROPERTY(EventStreamModel* logs READ logs CONSTANT)

public:
    struct Dependencies
    {
        EventStreamModel *logsModel = nullptr;
        EventController *eventController = nullptr;
    };

    explicit LogStreamFacade(Dependencies dependencies, QObject *parent = nullptr);

    EventStreamModel *logs();

    Q_INVOKABLE int loadOlderCurrentSessionLogs();
    Q_INVOKABLE void clearCurrentLogs();

signals:
    void logStreamChanged();
    void logStreamRowAppended(const QVariantMap &row);

private:
    Dependencies m_dependencies;
};
