#include "models/eventstreammodel.h"
#include "viewmodels/logscoreport.h"
#include "viewmodels/logsviewmodel.h"

#include <QtTest/QtTest>

class FakeLogsCore final : public LogsCorePort
{
public:
    void bindLogsSignals(QObject *, const LogsCoreSignalHandlers &newHandlers) override
    {
        handlers = newHandlers;
    }

    EventStreamModel *logs() override
    {
        return &model;
    }

    void clearCurrentLogs() override
    {
        clearCalled = true;
    }

    int loadOlderCurrentSessionLogs() override
    {
        ++loadCalls;
        return loadResult;
    }

    EventStreamModel model;
    LogsCoreSignalHandlers handlers;
    bool clearCalled = false;
    int loadCalls = 0;
    int loadResult = 0;
};

class LogsViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void formatsLogRows();
    void rendersLogTextFromModel();
    void delegatesCommandsToCorePort();
    void forwardsCorePortSignals();
};

void LogsViewModelTest::formatsLogRows()
{
    QCOMPARE(
        LogsViewModel::formattedLogRow(QVariantMap {
            {QStringLiteral("timestamp"), QStringLiteral("10:00:00")},
            {QStringLiteral("title"), QStringLiteral("mqtt")},
            {QStringLiteral("payload"), QStringLiteral("connected\nsession ready")},
        }),
        QStringLiteral("[10:00:00] [INFO] [mqtt] connected\n    session ready"));

    QCOMPARE(
        LogsViewModel::formattedLogRow(QVariantMap {
            {QStringLiteral("timestamp"), QStringLiteral("10:00:01")},
            {QStringLiteral("title"), QStringLiteral("warn")},
            {QStringLiteral("payload"), QStringLiteral("invalid topic")},
        }),
        QStringLiteral("[10:00:01] [WARN] invalid topic"));

    QCOMPARE(
        LogsViewModel::formattedLogRow(QVariantMap {
            {QStringLiteral("kind"), QStringLiteral("divider")},
            {QStringLiteral("title"), QStringLiteral("Current launch")},
        }),
        QStringLiteral("--- Current launch ---"));
}

void LogsViewModelTest::rendersLogTextFromModel()
{
    EventStreamModel model;
    model.appendRow(QVariantMap {
        {QStringLiteral("timestamp"), QStringLiteral("10:00:00")},
        {QStringLiteral("title"), QStringLiteral("debug")},
        {QStringLiteral("payload"), QStringLiteral("packet received")},
    });
    model.appendRow(QVariantMap {
        {QStringLiteral("timestamp"), QStringLiteral("10:00:01")},
        {QStringLiteral("title"), QStringLiteral("broker")},
        {QStringLiteral("payload"), QStringLiteral("timeout")},
    });

    QCOMPARE(
        LogsViewModel::renderedLogText(&model),
        QStringLiteral("[10:00:00] [DEBUG] packet received\n[10:00:01] [ERROR] [broker] timeout"));
}

void LogsViewModelTest::delegatesCommandsToCorePort()
{
    FakeLogsCore core;
    core.loadResult = 3;
    LogsViewModel viewModel(&core);

    viewModel.clearCurrentLogs();
    QCOMPARE(core.clearCalled, true);
    QCOMPARE(viewModel.loadOlderCurrentSessionLogs(), 3);
    QCOMPARE(core.loadCalls, 1);
}

void LogsViewModelTest::forwardsCorePortSignals()
{
    FakeLogsCore core;
    LogsViewModel viewModel(&core);
    QSignalSpy streamSpy(&viewModel, &LogsViewModel::logStreamChanged);
    QSignalSpy rowSpy(&viewModel, &LogsViewModel::logStreamRowAppended);
    QSignalSpy textSpy(&viewModel, &LogsViewModel::logTextChanged);

    QVERIFY(core.handlers.logStreamChanged);
    core.handlers.logStreamChanged();
    QCOMPARE(streamSpy.count(), 1);
    QCOMPARE(textSpy.count(), 1);

    const QVariantMap row {
        {QStringLiteral("timestamp"), QStringLiteral("10:00:00")},
        {QStringLiteral("title"), QStringLiteral("broker")},
        {QStringLiteral("payload"), QStringLiteral("connected")},
    };
    QVERIFY(core.handlers.logStreamRowAppended);
    core.handlers.logStreamRowAppended(row);
    QCOMPARE(rowSpy.count(), 1);
    QCOMPARE(rowSpy.takeFirst().at(0).toMap(), row);
    QCOMPARE(textSpy.count(), 2);
}

QTEST_MAIN(LogsViewModelTest)

#include "test_logsviewmodel.moc"
