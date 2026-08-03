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
    void emitsIncrementalTextChanges();
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

void LogsViewModelTest::emitsIncrementalTextChanges()
{
    LogsFixture fixture;
    QSignalSpy textSpy(&fixture.viewModel, &LogsViewModel::logTextChanged);
    QSignalSpy insertSpy(&fixture.viewModel, &LogsViewModel::logTextInserted);
    QSignalSpy removeSpy(&fixture.viewModel, &LogsViewModel::logTextRemoved);

    fixture.history.logStreamChanged();
    QCOMPARE(textSpy.count(), 1);

    fixture.logs.appendRow(QVariantMap {
        {QStringLiteral("timestamp"), QStringLiteral("10:00:00")},
        {QStringLiteral("title"), QStringLiteral("broker")},
        {QStringLiteral("payload"), QStringLiteral("connected")},
    });
    QCOMPARE(textSpy.count(), 2);
    QCOMPARE(insertSpy.count(), 1);
    QCOMPARE(insertSpy.first().at(0).toInt(), 0);
    QCOMPARE(insertSpy.first().at(1).toString(), QStringLiteral("[10:00:00] [INFO] [broker] connected"));
    QCOMPARE(fixture.viewModel.logText(), insertSpy.first().at(1).toString());

    fixture.logs.prependRows({QVariantMap {
        {QStringLiteral("timestamp"), QStringLiteral("09:59:59")},
        {QStringLiteral("title"), QStringLiteral("debug")},
        {QStringLiteral("payload"), QStringLiteral("packet")},
    }});
    QCOMPARE(textSpy.count(), 3);
    QCOMPARE(insertSpy.count(), 2);
    QCOMPARE(insertSpy.last().at(0).toInt(), 0);
    QCOMPARE(insertSpy.last().at(1).toString(), QStringLiteral("[09:59:59] [DEBUG] packet\n"));

    fixture.logs.trimToLimit(1);
    QCOMPARE(textSpy.count(), 4);
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(fixture.viewModel.logText(), QStringLiteral("[10:00:00] [INFO] [broker] connected"));
}

QTEST_MAIN(LogsViewModelTest)

#include "test_logsviewmodel.moc"
