#include "domain/session.h"
#include "models/eventstreammodel.h"
#include "services/storage/historystore.h"
#include "usecases/eventhistoryservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/scriptservice.h"
#include "usecases/sessionservice.h"
#include "viewmodels/logsviewmodel.h"

#include <QtTest/QtTest>

#include <QSettings>
#include <QTemporaryDir>

namespace {

SessionState &initializeSession(SessionService &sessions)
{
    SessionState session;
    session.id = QStringLiteral("session-1");
    session.name = QStringLiteral("Session 1");
    sessions.sessions().append(session);
    sessions.setCurrentSessionIndex(0);
    return *sessions.currentSession();
}

struct LogsFixture
{
    QTemporaryDir dataDir;
    QSettings settings;
    HistoryStore historyStore;
    PreferencesController preferences;
    EventStreamModel messages;
    EventStreamModel logs;
    ScriptService scripts;
    QString launchTimestamp = QStringLiteral("2026-07-25T00:00:00.000Z");
    SessionService sessions;
    SessionState &session;
    EventHistoryService history;
    LogsViewModel viewModel;

    LogsFixture()
        : settings(dataDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat)
        , historyStore(dataDir.path())
        , preferences(&settings)
        , sessions(settings, scripts, historyStore, preferences)
        , session(initializeSession(sessions))
        , history(
              sessions,
              historyStore,
              messages,
              logs,
              scripts,
              launchTimestamp,
              preferences)
        , viewModel(history, logs)
    {
    }
};

} // namespace

class LogsViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void formatsLogRows();
    void rendersLogTextFromModel();
    void routesCommandsToEventHistoryService();
    void forwardsEventHistorySignals();
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

void LogsViewModelTest::routesCommandsToEventHistoryService()
{
    LogsFixture fixture;
    QVERIFY2(fixture.historyStore.isReady(), qPrintable(fixture.historyStore.lastError()));

    fixture.historyStore.appendEvent(
        fixture.session.id,
        QStringLiteral("2026-07-24T10:00:00.000Z"),
        QStringLiteral("mqtt"),
        QStringLiteral("first"));
    fixture.historyStore.appendEvent(
        fixture.session.id,
        QStringLiteral("2026-07-24T10:00:01.000Z"),
        QStringLiteral("mqtt"),
        QStringLiteral("second"));
    const qint64 newestId = fixture.historyStore.appendEvent(
        fixture.session.id,
        QStringLiteral("2026-07-24T10:00:02.000Z"),
        QStringLiteral("mqtt"),
        QStringLiteral("third"));
    QVERIFY(newestId > 0);

    fixture.session.runtime.oldestLoadedLogId = newestId + 1;
    fixture.session.runtime.loadedAllLogHistory = false;

    QCOMPARE(fixture.viewModel.logs(), &fixture.logs);
    QCOMPARE(fixture.viewModel.loadOlderCurrentSessionLogs(), 3);
    QCOMPARE(fixture.logs.count(), 3);
    QCOMPARE(fixture.session.runtime.logRows.size(), 3);

    fixture.viewModel.clearCurrentLogs();
    QCOMPARE(fixture.logs.count(), 0);
    QCOMPARE(fixture.session.runtime.logRows.size(), 0);
    QCOMPARE(fixture.historyStore.loadLogs(fixture.session.id, 10).size(), 0);
}

void LogsViewModelTest::forwardsEventHistorySignals()
{
    LogsFixture fixture;
    QSignalSpy streamSpy(&fixture.viewModel, &LogsViewModel::logStreamChanged);
    QSignalSpy rowSpy(&fixture.viewModel, &LogsViewModel::logStreamRowAppended);
    QSignalSpy textSpy(&fixture.viewModel, &LogsViewModel::logTextChanged);

    fixture.history.logStreamChanged();
    QCOMPARE(streamSpy.count(), 1);
    QCOMPARE(textSpy.count(), 1);

    const QVariantMap row {
        {QStringLiteral("timestamp"), QStringLiteral("10:00:00")},
        {QStringLiteral("title"), QStringLiteral("broker")},
        {QStringLiteral("payload"), QStringLiteral("connected")},
    };
    fixture.logs.appendRow(row);
    QCOMPARE(textSpy.count(), 2);

    fixture.history.logAppended(row);
    QCOMPARE(rowSpy.count(), 1);
    QCOMPARE(rowSpy.takeFirst().at(0).toMap(), row);
    QCOMPARE(textSpy.count(), 2);
}

QTEST_MAIN(LogsViewModelTest)

#include "test_logsviewmodel.moc"
