#include "models/eventstreammodel.h"
#include "viewmodels/logsviewmodel.h"

#include <QtTest/QtTest>

#include <functional>
#include <utility>

class FakeLogsDeps
{
public:
    LogsViewModel::Dependencies dependencies()
    {
        return {
            &model,
            [this](QObject *, std::function<void()> handler) {
                logStreamChanged = std::move(handler);
            },
            [this](QObject *, std::function<void(const QVariantMap &)> handler) {
                logStreamRowAppended = std::move(handler);
            },
            [this]() {
                clearCalled = true;
            },
            [this]() {
                ++loadCalls;
                return loadResult;
            },
        };
    }

    EventStreamModel model;
    std::function<void()> logStreamChanged;
    std::function<void(const QVariantMap &)> logStreamRowAppended;
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
    void delegatesCommandsToDependencies();
    void forwardsDependencySignals();
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

void LogsViewModelTest::delegatesCommandsToDependencies()
{
    FakeLogsDeps deps;
    deps.loadResult = 3;
    LogsViewModel viewModel(deps.dependencies());

    viewModel.clearCurrentLogs();
    QCOMPARE(deps.clearCalled, true);
    QCOMPARE(viewModel.loadOlderCurrentSessionLogs(), 3);
    QCOMPARE(deps.loadCalls, 1);
}

void LogsViewModelTest::forwardsDependencySignals()
{
    FakeLogsDeps deps;
    LogsViewModel viewModel(deps.dependencies());
    QSignalSpy streamSpy(&viewModel, &LogsViewModel::logStreamChanged);
    QSignalSpy rowSpy(&viewModel, &LogsViewModel::logStreamRowAppended);
    QSignalSpy textSpy(&viewModel, &LogsViewModel::logTextChanged);

    QVERIFY(deps.logStreamChanged);
    deps.logStreamChanged();
    QCOMPARE(streamSpy.count(), 1);
    QCOMPARE(textSpy.count(), 1);

    const QVariantMap row {
        {QStringLiteral("timestamp"), QStringLiteral("10:00:00")},
        {QStringLiteral("title"), QStringLiteral("broker")},
        {QStringLiteral("payload"), QStringLiteral("connected")},
    };
    QVERIFY(deps.logStreamRowAppended);
    deps.logStreamRowAppended(row);
    QCOMPARE(rowSpy.count(), 1);
    QCOMPARE(rowSpy.takeFirst().at(0).toMap(), row);
    QCOMPARE(textSpy.count(), 2);
}

QTEST_MAIN(LogsViewModelTest)

#include "test_logsviewmodel.moc"
