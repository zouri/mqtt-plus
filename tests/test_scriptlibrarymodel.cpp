#include "domain/script.h"
#include "models/scriptlibrarymodel.h"

#include <QtTest/QtTest>

class ScriptLibraryModelTest : public QObject
{
    Q_OBJECT

private slots:
    void notifyRefreshUpdatesRowsWithoutResetWhenCountIsStable();
};

void ScriptLibraryModelTest::notifyRefreshUpdatesRowsWithoutResetWhenCountIsStable()
{
    ScriptEntry script;
    script.id = QStringLiteral("script-1");
    script.name = QStringLiteral("Decoder");
    script.description = QStringLiteral("Initial description");
    script.code = QStringLiteral("return payload");
    script.updatedAt = QStringLiteral("2026-07-04T00:00:00Z");
    script.fileName = QStringLiteral("script-1.lua");
    QVector<ScriptEntry> scripts {script};

    ScriptLibraryModel model;
    model.setSource(&scripts);

    QSignalSpy dataSpy(&model, &ScriptLibraryModel::dataChanged);
    QSignalSpy resetSpy(&model, &ScriptLibraryModel::modelReset);
    QSignalSpy countSpy(&model, &ScriptLibraryModel::countChanged);

    scripts[0].name = QStringLiteral("Pretty Decoder");
    model.notifyRefresh();

    QCOMPARE(model.rowAt(0).value(QStringLiteral("name")).toString(), QStringLiteral("Pretty Decoder"));
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(dataSpy.first().at(0).toModelIndex().row(), 0);
    QCOMPARE(dataSpy.first().at(1).toModelIndex().row(), 0);
}

QTEST_MAIN(ScriptLibraryModelTest)

#include "test_scriptlibrarymodel.moc"
