#include "controllers/sessionservice.h"
#include "domain/session.h"
#include "models/subscriptionlistmodel.h"

#include <QtTest/QtTest>

class SubscriptionListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void refreshBindsCurrentSession();
    void refreshRebuildsScriptNameCache();
};

void SubscriptionListModelTest::refreshBindsCurrentSession()
{
    SessionService sessions;
    SubscriptionListModel model;

    SessionState first;
    first.subscriptions.append({QStringLiteral("devices/first")});
    SessionState second;
    second.subscriptions.append({QStringLiteral("devices/second")});
    sessions.appendSession(first);
    sessions.appendSession(second);

    sessions.setCurrentIndex(0);
    model.setSource(sessions.currentSession());
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/first"));

    sessions.setCurrentIndex(1);
    model.setSource(sessions.currentSession());
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/second"));
}

void SubscriptionListModelTest::refreshRebuildsScriptNameCache()
{
    SessionService sessions;
    SubscriptionListModel model;
    QString scriptName = QStringLiteral("Decoder");

    model.setScriptNameLookup([&scriptName](const QString &id) {
        return id == QStringLiteral("script-1") ? scriptName : QString();
    });

    SessionState session;
    SubscriptionEntry subscription;
    subscription.topic = QStringLiteral("devices/temp");
    subscription.scriptId = QStringLiteral("script-1");
    session.subscriptions.append(subscription);
    sessions.appendSession(session);
    sessions.setCurrentIndex(0);

    model.setSource(sessions.currentSession());
    QCOMPARE(model.rowAt(0).value(QStringLiteral("scriptName")).toString(), QStringLiteral("Decoder"));

    scriptName = QStringLiteral("Pretty Decoder");
    model.setSource(sessions.currentSession());
    QCOMPARE(model.rowAt(0).value(QStringLiteral("scriptName")).toString(), QStringLiteral("Pretty Decoder"));
}

QTEST_MAIN(SubscriptionListModelTest)

#include "test_subscriptionlistmodel.moc"
