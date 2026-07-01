#include "app/applicationviewrefreshcoordinator.h"
#include "controllers/sessioncontroller.h"
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
    SessionController sessions;
    SubscriptionListModel model;
    ApplicationViewRefreshCoordinator coordinator;

    SessionState first;
    first.subscriptions.append({QStringLiteral("devices/first")});
    SessionState second;
    second.subscriptions.append({QStringLiteral("devices/second")});
    sessions.appendSession(first);
    sessions.appendSession(second);

    coordinator.setDependencies({
        nullptr,
        &sessions,
        nullptr,
        nullptr,
        nullptr,
        &model,
    });

    sessions.setCurrentIndex(0);
    coordinator.refreshSubscriptionsModel();
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/first"));

    sessions.setCurrentIndex(1);
    coordinator.refreshSubscriptionsModel();
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rowAt(0).value(QStringLiteral("topic")).toString(), QStringLiteral("devices/second"));
}

void SubscriptionListModelTest::refreshRebuildsScriptNameCache()
{
    SessionController sessions;
    SubscriptionListModel model;
    ApplicationViewRefreshCoordinator coordinator;
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

    coordinator.setDependencies({
        nullptr,
        &sessions,
        nullptr,
        nullptr,
        nullptr,
        &model,
    });

    coordinator.refreshSubscriptionsModel();
    QCOMPARE(model.rowAt(0).value(QStringLiteral("scriptName")).toString(), QStringLiteral("Decoder"));

    scriptName = QStringLiteral("Pretty Decoder");
    coordinator.refreshSubscriptionsModel();
    QCOMPARE(model.rowAt(0).value(QStringLiteral("scriptName")).toString(), QStringLiteral("Pretty Decoder"));
}

QTEST_MAIN(SubscriptionListModelTest)

#include "test_subscriptionlistmodel.moc"
