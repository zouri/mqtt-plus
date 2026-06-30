#include "viewmodels/scriptsviewmodel.h"

#include <QtTest/QtTest>

class ScriptsViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void matchesScriptFilter();
};

void ScriptsViewModelTest::matchesScriptFilter()
{
    QVERIFY(ScriptsViewModel::scriptMatchesFilter(
        QStringLiteral("Decoder"),
        QStringLiteral("Binary payload"),
        QStringLiteral("function parse(ctx)\nend\n"),
        QStringLiteral("decode")));
    QVERIFY(ScriptsViewModel::scriptMatchesFilter(
        QStringLiteral("Decoder"),
        QStringLiteral("Binary payload"),
        QStringLiteral("function parse(ctx)\nend\n"),
        QStringLiteral(" parse ")));
    QVERIFY(!ScriptsViewModel::scriptMatchesFilter(
        QStringLiteral("Decoder"),
        QStringLiteral("Binary payload"),
        QStringLiteral("function parse(ctx)\nend\n"),
        QStringLiteral("temperature")));
}

QTEST_MAIN(ScriptsViewModelTest)

#include "test_scriptsviewmodel.moc"
