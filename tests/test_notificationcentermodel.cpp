#include "models/notificationcentermodel.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class NotificationCenterModelTest : public QObject
{
    Q_OBJECT

private slots:
    void queuesBeyondThreeAndPromotesOnDismiss();
    void boundsQueuedNotifications();
    void preservesActionableNotificationDuringOverflow();
    void updatesInPlaceAndTriggersAction();
    void pausesAutomaticCloseWhileHovered();
};

void NotificationCenterModelTest::queuesBeyondThreeAndPromotesOnDismiss()
{
    NotificationCenterModel model;
    for (int index = 1; index <= 4; ++index) {
        model.postOrUpdate(
            QStringLiteral("id-%1").arg(index),
            QStringLiteral("Title %1").arg(index),
            QString(),
            QStringLiteral("info"));
    }
    QCOMPARE(model.count(), 3);

    model.dismiss(QStringLiteral("id-2"));
    QCOMPARE(model.count(), 3);
    QCOMPARE(model.index(2, 0).data(NotificationCenterModel::IdRole).toString(), QStringLiteral("id-4"));
}

void NotificationCenterModelTest::boundsQueuedNotifications()
{
    NotificationCenterModel model;
    for (int index = 1; index <= 40; ++index) {
        model.postOrUpdate(
            QStringLiteral("id-%1").arg(index),
            QStringLiteral("Title %1").arg(index),
            QString(),
            QStringLiteral("success"),
            4000);
    }

    QCOMPARE(model.count(), 3);
    model.dismiss(QStringLiteral("id-1"));
    QCOMPARE(model.index(2, 0).data(NotificationCenterModel::IdRole).toString(), QStringLiteral("id-9"));
}

void NotificationCenterModelTest::preservesActionableNotificationDuringOverflow()
{
    NotificationCenterModel model;
    for (int index = 1; index <= 3; ++index) {
        model.postOrUpdate(
            QStringLiteral("visible-%1").arg(index),
            QStringLiteral("Visible %1").arg(index),
            QString(),
            QStringLiteral("success"),
            4000);
    }
    model.postOrUpdate(
        QStringLiteral("storage-error"),
        QStringLiteral("Storage error"),
        QString(),
        QStringLiteral("error"),
        0,
        QStringLiteral("Restore"),
        QStringLiteral("restore"));
    for (int index = 1; index <= 40; ++index) {
        model.postOrUpdate(
            QStringLiteral("success-%1").arg(index),
            QStringLiteral("Success %1").arg(index),
            QString(),
            QStringLiteral("success"),
            4000);
    }

    model.dismiss(QStringLiteral("visible-1"));
    QCOMPARE(
        model.index(2, 0).data(NotificationCenterModel::IdRole).toString(),
        QStringLiteral("storage-error"));
}

void NotificationCenterModelTest::updatesInPlaceAndTriggersAction()
{
    NotificationCenterModel model;
    QSignalSpy actionSpy(&model, &NotificationCenterModel::actionRequested);
    model.postOrUpdate(
        QStringLiteral("publish:one"),
        QStringLiteral("Waiting"),
        QStringLiteral("Queued"),
        QStringLiteral("info"));
    model.postOrUpdate(
        QStringLiteral("publish:one"),
        QStringLiteral("Failed"),
        QStringLiteral("Rejected"),
        QStringLiteral("error"),
        0,
        QStringLiteral("View logs"),
        QStringLiteral("openLogs"));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.index(0, 0).data(NotificationCenterModel::TitleRole).toString(), QStringLiteral("Failed"));
    model.triggerAction(QStringLiteral("publish:one"));
    QCOMPARE(actionSpy.size(), 1);
    QCOMPARE(actionSpy.first().first().toString(), QStringLiteral("openLogs"));
    QCOMPARE(model.count(), 0);
}

void NotificationCenterModelTest::pausesAutomaticCloseWhileHovered()
{
    NotificationCenterModel model;
    model.postOrUpdate(
        QStringLiteral("success"),
        QStringLiteral("Saved"),
        QString(),
        QStringLiteral("success"),
        150);
    model.setHovered(QStringLiteral("success"), true);
    QTest::qWait(350);
    QCOMPARE(model.count(), 1);

    model.setHovered(QStringLiteral("success"), false);
    QTRY_COMPARE_WITH_TIMEOUT(model.count(), 0, 600);
}

QTEST_MAIN(NotificationCenterModelTest)

#include "test_notificationcentermodel.moc"
