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
};

void EventStreamModelTest::setRowsIgnoresUnchangedRows()
{
    EventStreamModel model;
    const QVariantList rows {
        QVariantMap {
            {QStringLiteral("id"), 1},
            {QStringLiteral("title"), QStringLiteral("first")},
        },
        QVariantMap {
            {QStringLiteral("id"), 2},
            {QStringLiteral("title"), QStringLiteral("second")},
        },
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
    model.setRows(QVariantList {
        QVariantMap {
            {QStringLiteral("id"), 1},
            {QStringLiteral("title"), QStringLiteral("first")},
        },
    });
    QSignalSpy countSpy(&model, &EventStreamModel::countChanged);
    QSignalSpy dataSpy(&model, &EventStreamModel::dataChanged);
    QSignalSpy resetSpy(&model, &EventStreamModel::modelReset);

    model.setRows(QVariantList {
        QVariantMap {
            {QStringLiteral("id"), 1},
            {QStringLiteral("title"), QStringLiteral("updated")},
        },
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
    model.setRows(QVariantList {
        QVariantMap {{QStringLiteral("id"), 1}},
        QVariantMap {{QStringLiteral("id"), 2}},
        QVariantMap {{QStringLiteral("id"), 3}},
    });

    QSignalSpy countSpy(&model, &EventStreamModel::countChanged);
    QSignalSpy removeSpy(&model, &EventStreamModel::rowsRemoved);

    model.trimToLimit(2);

    QCOMPARE(model.count(), 2);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(removeSpy.first().at(1).toInt(), 0);
    QCOMPARE(removeSpy.first().at(2).toInt(), 0);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("id")).toInt(), 2);
    QCOMPARE(model.rowAt(1).value(QStringLiteral("id")).toInt(), 3);
}

void EventStreamModelTest::updatesSingleHistoryRowWithoutReset()
{
    EventStreamModel model;
    model.setRows(QVariantList {
        QVariantMap {
            {QStringLiteral("historyId"), 41},
            {QStringLiteral("payload"), QStringLiteral("pending")},
            {QStringLiteral("parseState"), QStringLiteral("pending")},
        },
        QVariantMap {
            {QStringLiteral("historyId"), 42},
            {QStringLiteral("payload"), QStringLiteral("unchanged")},
        },
    });
    QSignalSpy dataSpy(&model, &EventStreamModel::dataChanged);
    QSignalSpy resetSpy(&model, &EventStreamModel::modelReset);

    QVERIFY(model.updateRowByHistoryId(
        41,
        QVariantMap {
            {QStringLiteral("historyId"), 41},
            {QStringLiteral("payload"), QStringLiteral("parsed")},
            {QStringLiteral("parseState"), QStringLiteral("succeeded")},
        }));

    QCOMPARE(model.rowAt(0).value(QStringLiteral("payload")).toString(), QStringLiteral("parsed"));
    QCOMPARE(model.rowAt(1).value(QStringLiteral("payload")).toString(), QStringLiteral("unchanged"));
    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(dataSpy.first().at(0).toModelIndex().row(), 0);
    QCOMPARE(dataSpy.first().at(1).toModelIndex().row(), 0);
    QCOMPARE(resetSpy.count(), 0);
}

void EventStreamModelTest::appendRowsAndTrimFrontKeepsIncrementalWindow()
{
    EventStreamModel model;
    model.setRows(QVariantList {
        QVariantMap {{QStringLiteral("id"), 1}},
        QVariantMap {{QStringLiteral("id"), 2}},
        QVariantMap {{QStringLiteral("id"), 3}},
    });

    QSignalSpy countSpy(&model, &EventStreamModel::countChanged);
    QSignalSpy insertSpy(&model, &EventStreamModel::rowsInserted);
    QSignalSpy removeSpy(&model, &EventStreamModel::rowsRemoved);
    QSignalSpy resetSpy(&model, &EventStreamModel::modelReset);
    QSignalSpy dataSpy(&model, &EventStreamModel::dataChanged);

    QCOMPARE(
        model.appendRowsAndTrimFront(
            QVariantList {
                QVariantMap {{QStringLiteral("id"), 4}},
                QVariantMap {{QStringLiteral("id"), 5}},
            },
            3),
        2);

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("id")).toInt(), 3);
    QCOMPARE(model.rowAt(2).value(QStringLiteral("id")).toInt(), 5);
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
    model.setRows(QVariantList {
        QVariantMap {{QStringLiteral("id"), 3}},
        QVariantMap {{QStringLiteral("id"), 4}},
        QVariantMap {{QStringLiteral("id"), 5}},
    });

    QSignalSpy countSpy(&model, &EventStreamModel::countChanged);
    QSignalSpy insertSpy(&model, &EventStreamModel::rowsInserted);
    QSignalSpy removeSpy(&model, &EventStreamModel::rowsRemoved);
    QSignalSpy resetSpy(&model, &EventStreamModel::modelReset);
    QSignalSpy dataSpy(&model, &EventStreamModel::dataChanged);

    QCOMPARE(
        model.prependRowsAndTrimBack(
            QVariantList {
                QVariantMap {{QStringLiteral("id"), 1}},
                QVariantMap {{QStringLiteral("id"), 2}},
            },
            3),
        2);

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(model.rowAt(2).value(QStringLiteral("id")).toInt(), 3);
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
    const QVariantMap row {
        {QStringLiteral("id"), 1},
        {QStringLiteral("title"), QStringLiteral("one")},
    };
    model.appendRow(row);

    QVERIFY(model.lastRowEquals(row));
    QVERIFY(!model.lastRowEquals(QVariantMap {
        {QStringLiteral("id"), 2},
        {QStringLiteral("title"), QStringLiteral("two")},
    }));
}

void EventStreamModelTest::exposesCanonicalMessageMetadata()
{
    EventStreamModel model;
    model.appendRow(QVariantMap {
        {QStringLiteral("direction"), QStringLiteral("outgoing")},
        {QStringLiteral("alias"), QStringLiteral("Living room light")},
        {QStringLiteral("qos"), 1},
        {QStringLiteral("retain"), true},
        {QStringLiteral("retainKnown"), true},
        {QStringLiteral("parsedPayload"), QStringLiteral("on")},
        {QStringLiteral("historyId"), 42},
        {QStringLiteral("payloadState"), QStringLiteral("full")},
        {QStringLiteral("payloadHash"), QStringLiteral("abc")},
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

QTEST_MAIN(EventStreamModelTest)

#include "test_eventstreammodel.moc"
