#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QMap>
#include <QStringList>

class ArchitectureBoundariesTest : public QObject
{
    Q_OBJECT

private slots:
    void controllersDoNotDependOnApplicationCore();
    void controllerHeadersUseDedicatedContexts();
    void applicationCoreDoesNotFriendControllers();
    void applicationCoreDoesNotOwnPlatformActions();
    void addSubscriptionDialogDoesNotBuildScriptOptions();

private:
    bool readSourceFile(const QString &relativePath, QString &source) const;
};

bool ArchitectureBoundariesTest::readSourceFile(const QString &relativePath, QString &source) const
{
    QFile file(QStringLiteral(MQTT_PLUS_SOURCE_DIR) + QLatin1Char('/') + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    source = QString::fromUtf8(file.readAll());
    return true;
}

void ArchitectureBoundariesTest::controllersDoNotDependOnApplicationCore()
{
    const QStringList controllerHeaders {
        QStringLiteral("src/controllers/eventcontroller.h"),
        QStringLiteral("src/controllers/mqttcontroller.h"),
        QStringLiteral("src/controllers/sessioncontroller.h"),
        QStringLiteral("src/controllers/subscriptioncontroller.h"),
    };

    for (const QString &header : controllerHeaders) {
        QString source;
        QVERIFY2(readSourceFile(header, source), qPrintable(QStringLiteral("Cannot read %1").arg(header)));
        QVERIFY2(!source.contains(QStringLiteral("ApplicationCore")),
            qPrintable(QStringLiteral("%1 must depend on a narrow controller context, not ApplicationCore").arg(header)));
    }
}

void ArchitectureBoundariesTest::controllerHeadersUseDedicatedContexts()
{
    const QMap<QString, QString> expectedContexts {
        {QStringLiteral("src/controllers/eventcontroller.h"), QStringLiteral("EventControllerContext")},
        {QStringLiteral("src/controllers/mqttcontroller.h"), QStringLiteral("MqttControllerContext")},
        {QStringLiteral("src/controllers/sessioncontroller.h"), QStringLiteral("SessionControllerContext")},
        {QStringLiteral("src/controllers/subscriptioncontroller.h"), QStringLiteral("SubscriptionControllerContext")},
    };

    for (auto it = expectedContexts.cbegin(); it != expectedContexts.cend(); ++it) {
        QString source;
        QVERIFY2(readSourceFile(it.key(), source), qPrintable(QStringLiteral("Cannot read %1").arg(it.key())));
        QVERIFY2(source.contains(it.value()),
            qPrintable(QStringLiteral("%1 must depend on %2").arg(it.key(), it.value())));
        QVERIFY2(!source.contains(QStringLiteral("ApplicationContext")),
            qPrintable(QStringLiteral("%1 must not depend on the aggregate ApplicationContext").arg(it.key())));
    }
}

void ArchitectureBoundariesTest::applicationCoreDoesNotFriendControllers()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("src/app/applicationcore.h"), source));

    QVERIFY(!source.contains(QStringLiteral("friend class MqttController")));
    QVERIFY(!source.contains(QStringLiteral("friend class SubscriptionController")));
    QVERIFY(!source.contains(QStringLiteral("friend class EventController")));
    QVERIFY(!source.contains(QStringLiteral("friend class SessionController")));
}

void ArchitectureBoundariesTest::applicationCoreDoesNotOwnPlatformActions()
{
    const QMap<QString, QStringList> forbiddenTokens {
        {
            QStringLiteral("src/app/applicationcore.cpp"),
            {
                QStringLiteral("QClipboard"),
                QStringLiteral("QGuiApplication"),
            },
        },
        {
            QStringLiteral("src/app/applicationcoremenus.cpp"),
            {
                QStringLiteral("QAction"),
                QStringLiteral("QCursor"),
                QStringLiteral("QIcon"),
                QStringLiteral("QMenu"),
            },
        },
    };

    for (auto it = forbiddenTokens.cbegin(); it != forbiddenTokens.cend(); ++it) {
        QString source;
        QVERIFY2(readSourceFile(it.key(), source), qPrintable(QStringLiteral("Cannot read %1").arg(it.key())));
        for (const QString &token : it.value()) {
            QVERIFY2(!source.contains(token),
                qPrintable(QStringLiteral("%1 must delegate platform action API %2").arg(it.key(), token)));
        }
    }
}

void ArchitectureBoundariesTest::addSubscriptionDialogDoesNotBuildScriptOptions()
{
    QString source;
    QVERIFY(readSourceFile(QStringLiteral("qml/features/workbench/AddSubscriptionDialog.qml"), source));

    QVERIFY2(!source.contains(QStringLiteral("rowAt(")),
        "AddSubscriptionDialog.qml must not read script model rows to build editor options");
    QVERIFY2(!source.contains(QStringLiteral("setScriptOptions")),
        "AddSubscriptionDialog.qml must not push script options into the editor ViewModel");
    QVERIFY2(!source.contains(QStringLiteral("scriptLibraryChanged")),
        "AddSubscriptionDialog.qml must not synchronize script library state");
}

QTEST_MAIN(ArchitectureBoundariesTest)

#include "test_architecture_boundaries.moc"
