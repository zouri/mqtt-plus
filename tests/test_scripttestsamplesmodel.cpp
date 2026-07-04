#include "models/scripttestsamplesmodel.h"

#include <QtTest/QtTest>

class ScriptTestSamplesModelTest : public QObject
{
    Q_OBJECT

private slots:
    void notifyRefreshUpdatesRowsWithoutResetWhenCountIsStable();
};

namespace {

QVariantMap sampleRow(const QString &topic, const QString &payload)
{
    return {
        {QStringLiteral("topic"), topic},
        {QStringLiteral("testPayload"), payload},
        {QStringLiteral("testFormat"), 1},
        {QStringLiteral("testFormatName"), QStringLiteral("Text")},
        {QStringLiteral("timestamp"), QStringLiteral("00:00:00")},
        {QStringLiteral("payloadSize"), payload.size()},
    };
}

} // namespace

void ScriptTestSamplesModelTest::notifyRefreshUpdatesRowsWithoutResetWhenCountIsStable()
{
    QVariantList messages {
        sampleRow(QStringLiteral("devices/temp"), QStringLiteral("23")),
    };
    ScriptTestSamplesModel model;
    model.setSource(&messages);

    QSignalSpy dataSpy(&model, &ScriptTestSamplesModel::dataChanged);
    QSignalSpy resetSpy(&model, &ScriptTestSamplesModel::modelReset);
    QSignalSpy countSpy(&model, &ScriptTestSamplesModel::countChanged);

    messages[0] = sampleRow(QStringLiteral("devices/temp"), QStringLiteral("24"));
    model.notifyRefresh();

    QCOMPARE(model.rowAt(0).value(QStringLiteral("testPayload")).toString(), QStringLiteral("24"));
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(dataSpy.first().at(0).toModelIndex().row(), 0);
    QCOMPARE(dataSpy.first().at(1).toModelIndex().row(), 0);
}

QTEST_MAIN(ScriptTestSamplesModelTest)

#include "test_scripttestsamplesmodel.moc"
