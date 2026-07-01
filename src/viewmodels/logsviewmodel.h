#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include <functional>

class EventStreamModel;

class LogsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(EventStreamModel* logs READ logs CONSTANT)
    Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)

public:
    struct Dependencies
    {
        EventStreamModel *logs = nullptr;
        std::function<void(QObject *, std::function<void()>)> bindLogStreamChanged;
        std::function<void(QObject *, std::function<void(const QVariantMap &)>)> bindLogStreamRowAppended;
        std::function<void()> clearCurrentLogs;
        std::function<int()> loadOlderCurrentSessionLogs;
    };

    explicit LogsViewModel(QObject *parent = nullptr);
    explicit LogsViewModel(const Dependencies &dependencies, QObject *parent = nullptr);

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
    Dependencies m_dependencies;
};
