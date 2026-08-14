#include "models/eventstreammodel.h"

#include <QtTest/QtTest>

class EventStreamModelTest : public QObject
{
    Q_OBJECT

private slots:
    void setRowsIgnoresUnchangedRows();
    void setRowsUpdatesRowsWithoutResetWhenCountIsStable();
    void trimToLimitRemovesOldestRows();
    void appendRowsAndTrimFrontKeepsIncrementalWindow();
    void prependRowsAndTrimBackKeepsIncrementalWindow();
    void updatesSingleHistoryRowWithoutReset();
    void detectsMatchingLastRow();
    void exposesCanonicalMessageMetadata();
    void rowAtMatchesRoleInterface();
    void maintainsMessageCountIncrementally();
    void cachesExpandedPayloadByHistoryId();
};

void EventStreamModelTest::setRowsIgnoresUnchangedRows()
{
    EventStreamModel model;
    const QVector<EventRow> rows {
        EventRow {.title = QStringLiteral("first"), .historyId = 1},
        EventRow {.title = QStringLiteral("second"), .historyId = 2},
    };
    model.setRows(rows);
    QSignalSpy countSpy(&model, &EventStreamModel::countChanged);
    QSignalSpy resetSpy(&model, &EventStreamModel::modelReset);

    model.setRows(rows);

    QCOMPARE(model.count(), 2);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(resetSpy.count(), 0);
}

void EventStreamModelTest::setRowsUpdatesRowsWithoutResetWhenCountIsStable()
{
    EventStreamModel model;
    model.setRows(QVector<EventRow> {
        EventRow {.title = QStringLiteral("first"), .historyId = 1},
    });
    QSignalSpy countSpy(&model, &EventStreamModel::countChanged);
    QSignalSpy dataSpy(&model, &EventStreamModel::dataChanged);
    QSignalSpy resetSpy(&model, &EventStreamModel::modelReset);

    model.setRows(QVector<EventRow> {
        EventRow {.title = QStringLiteral("updated"), .historyId = 1},
    });

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("title")).toString(), QStringLiteral("updated"));
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(dataSpy.first().at(0).toModelIndex().row(), 0);
    QCOMPARE(dataSpy.first().at(1).toModelIndex().row(), 0);
}

void EventStreamModelTest::trimToLimitRemovesOldestRows()
{
    EventStreamModel model;
    model.setRows(QVector<EventRow> {
        EventRow {.historyId = 1},
        EventRow {.historyId = 2},
        EventRow {.historyId = 3},
    });

    QSignalSpy countSpy(&model, &EventStreamModel::countChanged);
    QSignalSpy removeSpy(&model, &EventStreamModel::rowsRemoved);

    model.trimToLimit(2);

    QCOMPARE(model.count(), 2);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(removeSpy.first().at(1).toInt(), 0);
    QCOMPARE(removeSpy.first().at(2).toInt(), 0);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("historyId")).toLongLong(), 2);
    QCOMPARE(model.rowAt(1).value(QStringLiteral("historyId")).toLongLong(), 3);
}

void EventStreamModelTest::updatesSingleHistoryRowWithoutReset()
{
    EventStreamModel model;
    model.setRows(QVector<EventRow> {
        EventRow {
            .payload = QStringLiteral("pending"),
            .historyId = 41,
            .parseState = QStringLiteral("pending"),
        },
        EventRow {.payload = QStringLiteral("unchanged"), .historyId = 42},
    });
    QSignalSpy dataSpy(&model, &EventStreamModel::dataChanged);
    QSignalSpy resetSpy(&model, &EventStreamModel::modelReset);

    QVERIFY(model.updateRowByHistoryId(
        41,
        EventRow {
            .payload = QStringLiteral("parsed"),
            .historyId = 41,
            .parseState = QStringLiteral("succeeded"),
        }));

    QCOMPARE(model.rowAt(0).value(QStringLiteral("payload")).toString(), QStringLiteral("parsed"));
    QCOMPARE(model.rowAt(1).value(QStringLiteral("payload")).toString(), QStringLiteral("unchanged"));
    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(dataSpy.first().at(0).toModelIndex().row(), 0);
    QCOMPARE(dataSpy.first().at(1).toModelIndex().row(), 0);
    QCOMPARE(resetSpy.count(), 0);
}

void EventStreamModelTest::cachesExpandedPayloadByHistoryId()
{
    EventStreamModel model;
    model.setRows(QVector<EventRow> {
        EventRow {
            .kind = QStringLiteral("message"),
            .payload = QStringLiteral("preview"),
            .historyId = 41,
            .expandedPayloadNeeded = true,
        },
    });
    QSignalSpy dataSpy(&model, &EventStreamModel::dataChanged);

    QVERIFY(model.beginExpandedPayloadLoad(41));
    QCOMPARE(
        model.rowAt(0).value(QStringLiteral("expandedPayloadState")).toString(),
        QStringLiteral("loading"));
    QVERIFY(!model.beginExpandedPayloadLoad(41));

    QVERIFY(model.finishExpandedPayloadLoad(
        41,
        QStringLiteral("complete payload"),
        QStringLiteral("ready")));
    const QVariantMap row = model.rowAt(0);
    QCOMPARE(row.value(QStringLiteral("payload")).toString(), QStringLiteral("preview"));
    QCOMPARE(row.value(QStringLiteral("expandedPayload")).toString(), QStringLiteral("complete payload"));
    QCOMPARE(row.value(QStringLiteral("expandedPayloadState")).toString(), QStringLiteral("ready"));
    QCOMPARE(dataSpy.count(), 2);
}

void EventStreamModelTest::appendRowsAndTrimFrontKeepsIncrementalWindow()
{
    EventStreamModel model;
    model.setRows(QVector<EventRow> {
        EventRow {.historyId = 1},
        EventRow {.historyId = 2},
        EventRow {.historyId = 3},
    });

    QSignalSpy countSpy(&model, &EventStreamModel::countChanged);
    QSignalSpy insertSpy(&model, &EventStreamModel::rowsInserted);
    QSignalSpy removeSpy(&model, &EventStreamModel::rowsRemoved);
    QSignalSpy resetSpy(&model, &EventStreamModel::modelReset);
    QSignalSpy dataSpy(&model, &EventStreamModel::dataChanged);

    QCOMPARE(
        model.appendRowsAndTrimFront(
            QVector<EventRow> {
                EventRow {.historyId = 4},
                EventRow {.historyId = 5},
            },
            3),
        2);

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("historyId")).toLongLong(), 3);
    QCOMPARE(model.rowAt(2).value(QStringLiteral("historyId")).toLongLong(), 5);
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(removeSpy.first().at(1).toInt(), 0);
    QCOMPARE(removeSpy.first().at(2).toInt(), 1);
    QCOMPARE(insertSpy.count(), 1);
    QCOMPARE(insertSpy.first().at(1).toInt(), 1);
    QCOMPARE(insertSpy.first().at(2).toInt(), 2);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(dataSpy.count(), 0);
}

void EventStreamModelTest::prependRowsAndTrimBackKeepsIncrementalWindow()
{
    EventStreamModel model;
    model.setRows(QVector<EventRow> {
        EventRow {.historyId = 3},
        EventRow {.historyId = 4},
        EventRow {.historyId = 5},
    });

    QSignalSpy countSpy(&model, &EventStreamModel::countChanged);
    QSignalSpy insertSpy(&model, &EventStreamModel::rowsInserted);
    QSignalSpy removeSpy(&model, &EventStreamModel::rowsRemoved);
    QSignalSpy resetSpy(&model, &EventStreamModel::modelReset);
    QSignalSpy dataSpy(&model, &EventStreamModel::dataChanged);

    QCOMPARE(
        model.prependRowsAndTrimBack(
            QVector<EventRow> {
                EventRow {.historyId = 1},
                EventRow {.historyId = 2},
            },
            3),
        2);

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("historyId")).toLongLong(), 1);
    QCOMPARE(model.rowAt(2).value(QStringLiteral("historyId")).toLongLong(), 3);
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(removeSpy.first().at(1).toInt(), 1);
    QCOMPARE(removeSpy.first().at(2).toInt(), 2);
    QCOMPARE(insertSpy.count(), 1);
    QCOMPARE(insertSpy.first().at(1).toInt(), 0);
    QCOMPARE(insertSpy.first().at(2).toInt(), 1);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(dataSpy.count(), 0);
}

void EventStreamModelTest::detectsMatchingLastRow()
{
    EventStreamModel model;
    const EventRow row {.title = QStringLiteral("one"), .historyId = 1};
    model.appendRow(row);

    QVERIFY(model.lastRowEquals(row));
    QVERIFY(!model.lastRowEquals(
        EventRow {.title = QStringLiteral("two"), .historyId = 2}));
}

void EventStreamModelTest::maintainsMessageCountIncrementally()
{
    EventStreamModel model;
    model.setRows(QVector<EventRow> {
        EventRow {.kind = QStringLiteral("divider")},
        EventRow {.kind = QStringLiteral("message"), .historyId = 1},
        EventRow {.kind = QStringLiteral("message"), .historyId = 2},
    });
    QCOMPARE(model.messageCount(), 2);
    QSignalSpy messageCountSpy(&model, &EventStreamModel::messageCountChanged);

    model.appendRow(EventRow {.kind = QStringLiteral("message")});
    QCOMPARE(model.messageCount(), 3);
    QCOMPARE(messageCountSpy.count(), 1);

    model.trimToLimit(2);
    QCOMPARE(model.messageCount(), 2);
    QCOMPARE(messageCountSpy.count(), 2);

    QVERIFY(model.updateRowByHistoryId(
        2,
        EventRow {.kind = QStringLiteral("divider"), .historyId = 2}));
    QCOMPARE(model.messageCount(), 1);
    QCOMPARE(messageCountSpy.count(), 3);

    model.clear();
    QCOMPARE(model.messageCount(), 0);
    QCOMPARE(messageCountSpy.count(), 4);
}

void EventStreamModelTest::exposesCanonicalMessageMetadata()
{
    EventStreamModel model;
    model.appendRow(EventRow {
        .historyId = 42,
        .direction = QStringLiteral("outgoing"),
        .alias = QStringLiteral("Living room light"),
        .qos = 1,
        .retain = true,
        .retainKnown = true,
        .parsedPayload = QStringLiteral("on"),
        .payloadState = QStringLiteral("full"),
        .payloadHash = QStringLiteral("abc"),
    });

    QCOMPARE(model.data(model.index(0, 0), EventStreamModel::DirectionRole).toString(), QStringLiteral("outgoing"));
    QCOMPARE(model.data(model.index(0, 0), EventStreamModel::AliasRole).toString(), QStringLiteral("Living room light"));
    QCOMPARE(model.data(model.index(0, 0), EventStreamModel::QosRole).toInt(), 1);
    QCOMPARE(model.data(model.index(0, 0), EventStreamModel::RetainRole).toBool(), true);
    QCOMPARE(model.data(model.index(0, 0), EventStreamModel::RetainKnownRole).toBool(), true);
    QCOMPARE(model.data(model.index(0, 0), EventStreamModel::HistoryIdRole).toLongLong(), 42);
    QCOMPARE(model.data(model.index(0, 0), EventStreamModel::PayloadStateRole).toString(), QStringLiteral("full"));
    QCOMPARE(model.data(model.index(0, 0), EventStreamModel::PayloadHashRole).toString(), QStringLiteral("abc"));
    QCOMPARE(model.rowAt(0).value(QStringLiteral("parsedPayload")).toString(), QStringLiteral("on"));
}

void EventStreamModelTest::rowAtMatchesRoleInterface()
{
    EventStreamModel model;
    model.appendRow(EventRow {
        .kind = QStringLiteral("message"),
        .timestamp = QStringLiteral("12:34:56"),
        .timestampRaw = QStringLiteral("2026-08-14T12:34:56.000Z"),
        .title = QStringLiteral("devices/temperature"),
        .payload = QStringLiteral("23"),
        .payloadFormat = QStringLiteral("Plaintext"),
        .payloadSize = 2,
        .topic = QStringLiteral("devices/temperature"),
        .topicColor = QStringLiteral("#112233"),
        .testPayload = QStringLiteral("23"),
        .testFormat = 1,
        .testFormatName = QStringLiteral("Plaintext"),
        .historyId = 9007199254740991LL,
        .direction = QStringLiteral("incoming"),
        .alias = QStringLiteral("Temperature"),
        .qos = 1,
        .retain = true,
        .retainKnown = true,
        .parsedPayload = QStringLiteral("23"),
        .parseState = QStringLiteral("succeeded"),
        .payloadState = QStringLiteral("full"),
        .payloadHash = QStringLiteral("abc"),
        .expandedPayload = QStringLiteral("23"),
        .expandedPayloadState = QStringLiteral("ready"),
        .expandedPayloadNeeded = true,
    });

    const QModelIndex index = model.index(0, 0);
    const QHash<int, QByteArray> roles = model.roleNames();
    const QVariantMap row = model.rowAt(0);
    QCOMPARE(row.size(), roles.size());
    for (auto role = roles.cbegin(); role != roles.cend(); ++role) {
        QCOMPARE(row.value(QString::fromLatin1(role.value())), model.data(index, role.key()));
    }
    QCOMPARE(
        row.value(QStringLiteral("historyId")).metaType().id(),
        int(QMetaType::QString));
    QVERIFY(!row.contains(QStringLiteral("timestampRaw")));
}

QTEST_MAIN(EventStreamModelTest)

#include "test_eventstreammodel.moc"
