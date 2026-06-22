#include "models/eventstreammodel.h"

#include <QSignalSpy>
#include <QTest>

class EventStreamModelTest : public QObject
{
    Q_OBJECT

private slots:
    void appendRowsAddsContiguousBatch()
    {
        EventStreamModel model;
        QSignalSpy countSpy(&model, &EventStreamModel::countChanged);

        QVariantMap first;
        first.insert(QStringLiteral("id"), 1);
        first.insert(QStringLiteral("title"), QStringLiteral("first"));

        QVariantMap second;
        second.insert(QStringLiteral("id"), 2);
        second.insert(QStringLiteral("title"), QStringLiteral("second"));

        model.appendRows(QVariantList { first, second });

        QCOMPARE(model.count(), 2);
        QCOMPARE(countSpy.count(), 1);
        QCOMPARE(model.rowAt(0).value(QStringLiteral("title")).toString(), QStringLiteral("first"));
        QCOMPARE(model.rowAt(1).value(QStringLiteral("title")).toString(), QStringLiteral("second"));
    }

    void trimToLimitRemovesOldestRowsAfterBatchAppend()
    {
        EventStreamModel model;

        QVariantList rows;
        for (int i = 0; i < 5; ++i) {
            QVariantMap row;
            row.insert(QStringLiteral("id"), i);
            rows.append(row);
        }

        model.appendRows(rows);
        model.trimToLimit(3);

        QCOMPARE(model.count(), 3);
        QCOMPARE(model.rowAt(0).value(QStringLiteral("id")).toInt(), 2);
        QCOMPARE(model.rowAt(2).value(QStringLiteral("id")).toInt(), 4);
    }
};

QTEST_MAIN(EventStreamModelTest)

#include "test_eventstreammodel.moc"
