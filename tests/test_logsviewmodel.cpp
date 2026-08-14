#include "domain/session.h"
#include "models/eventstreammodel.h"
#include "services/storage/historystore.h"
#include "services/storage/historywriterworker.h"
#include "services/parsing/messageparseworker.h"
#include "services/processors/processorlibrary.h"
#include "usecases/eventhistoryservice.h"
#include "usecases/preferencescontroller.h"
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
    HistoryWriterWorker historyWriter;
    MessageParseWorker messageParser;
    PreferencesController preferences;
    EventStreamModel messages;
    EventStreamModel logs;
    ProcessorLibrary processors;
    QString launchTimestamp = QStringLiteral("2026-07-25T00:00:00.000Z");
    SessionService sessions;
    SessionState &session;
    EventHistoryService history;
    LogsViewModel viewModel;

    LogsFixture()
        : settings(dataDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat)
        , historyStore(dataDir.path())
        , historyWriter(dataDir.path(), historyStore.nextMessageId())
        , preferences(&settings)
        , processors(dataDir.filePath(QStringLiteral("processors")))
        , sessions(settings, historyStore, preferences)
        , session(initializeSession(sessions))
        , history(
              sessions,
              historyStore,
              historyWriter,
              messageParser,
              messages,
              logs,
              processors,
              launchTimestamp,
              preferences)
        , viewModel(history, logs)
    {
        historyWriter.start();
        messageParser.start();
        sessions.setHistoryWriter(&historyWriter);
        sessions.setMessageParser(&messageParser);
    }
};

} // namespace

class LogsViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void formatsLogRows();
    void emitsIncrementalTextChanges();
};

void LogsViewModelTest::formatsLogRows()
{
    LogsFixture fixture;
    fixture.logs.setRows(QVector<EventRow> {
        EventRow {
            .timestamp = QStringLiteral("10:00:00"),
            .title = QStringLiteral("mqtt"),
            .payload = QStringLiteral("connected\nsession ready"),
        },
        EventRow {
            .timestamp = QStringLiteral("10:00:01"),
            .title = QStringLiteral("warn"),
            .payload = QStringLiteral("invalid topic"),
        },
        EventRow {
            .kind = QStringLiteral("divider"),
            .title = QStringLiteral("Current launch"),
        },
    });

    QCOMPARE(
        fixture.viewModel.logText(),
        QStringLiteral(
            "[10:00:00] [INFO] [mqtt] connected\n"
            "    session ready\n"
            "[10:00:01] [WARN] invalid topic\n"
            "--- Current launch ---"));
}

void LogsViewModelTest::emitsIncrementalTextChanges()
{
    LogsFixture fixture;
    QSignalSpy textSpy(&fixture.viewModel, &LogsViewModel::logTextChanged);
    QSignalSpy insertSpy(&fixture.viewModel, &LogsViewModel::logTextInserted);
    QSignalSpy removeSpy(&fixture.viewModel, &LogsViewModel::logTextRemoved);

    fixture.history.logStreamChanged();
    QCOMPARE(textSpy.count(), 1);

    fixture.logs.appendRow(EventRow {
        .timestamp = QStringLiteral("10:00:00"),
        .title = QStringLiteral("broker"),
        .payload = QStringLiteral("connected"),
    });
    QCOMPARE(textSpy.count(), 2);
    QCOMPARE(insertSpy.count(), 1);
    QCOMPARE(insertSpy.first().at(0).toInt(), 0);
    QCOMPARE(insertSpy.first().at(1).toString(), QStringLiteral("[10:00:00] [INFO] [broker] connected"));
    QCOMPARE(fixture.viewModel.logText(), insertSpy.first().at(1).toString());

    fixture.logs.prependRowsAndTrimBack(
        {EventRow {
            .timestamp = QStringLiteral("09:59:59"),
            .title = QStringLiteral("debug"),
            .payload = QStringLiteral("packet"),
        }},
        2);
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
