#pragma once

#include <functional>

#include <QVariantMap>

class EventStreamModel;
class QObject;

struct LogsCoreSignalHandlers
{
    std::function<void()> logStreamChanged;
    std::function<void(const QVariantMap &)> logStreamRowAppended;
};

class LogsCorePort
{
public:
    virtual ~LogsCorePort() = default;

    virtual void bindLogsSignals(QObject *context, const LogsCoreSignalHandlers &handlers) = 0;
    virtual EventStreamModel *logs() = 0;
    virtual void clearCurrentLogs() = 0;
    virtual int loadOlderCurrentSessionLogs() = 0;
};
