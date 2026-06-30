#include "viewmodels/applicationviewmodel.h"

#include <QtTest/QtTest>

class ApplicationViewModelShapeTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesFeatureViewModels();
};

void ApplicationViewModelShapeTest::exposesFeatureViewModels()
{
    ApplicationViewModel app;

    QVERIFY(app.navigation());
    QVERIFY(app.workbench());
    QVERIFY(app.logs());
    QVERIFY(app.scripts());
    QVERIFY(app.settings());
}

QTEST_MAIN(ApplicationViewModelShapeTest)

#include "test_applicationviewmodel_shape.moc"
