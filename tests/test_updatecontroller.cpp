#include "services/update/updateservice.h"
#include "usecases/updatecontroller.h"

#include <QtTest/QtTest>

#include <QSettings>
#include <QTemporaryDir>

class FakeUpdateService : public UpdateService
{
public:
    void fetchLatestRelease() override
    {
        ++fetchCount;
    }

    bool openRelease(const UpdateRelease &release) const override
    {
        ++openCount;
        openedRelease = release;
        return openResult;
    }

    void succeed(const UpdateRelease &release)
    {
        emit releaseFetched(release);
    }

    void fail(Error error)
    {
        emit fetchFailed(error);
    }

    int fetchCount = 0;
    mutable int openCount = 0;
    mutable UpdateRelease openedRelease;
    bool openResult = true;
};

class UpdateControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void failedAutomaticChecksAreThrottled();
    void manualChecksIgnoreAutomaticThrottle();
    void exposesAvailableReleaseAndOpensIt();
    void doesNotOfferCurrentOrInvalidRelease();
};

void UpdateControllerTest::failedAutomaticChecksAreThrottled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    FakeUpdateService service;
    UpdateController controller(settings, service, QStringLiteral("0.1.0"));

    controller.checkForUpdates(UpdateController::CheckMode::Automatic);
    QCOMPARE(service.fetchCount, 1);
    service.fail(UpdateService::Error::Network);
    QCOMPARE(controller.status(), UpdateController::Status::NetworkError);

    controller.checkForUpdates(UpdateController::CheckMode::Automatic);
    QCOMPARE(service.fetchCount, 1);
}

void UpdateControllerTest::manualChecksIgnoreAutomaticThrottle()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    FakeUpdateService service;
    UpdateController controller(settings, service, QStringLiteral("0.1.0"));

    controller.checkForUpdates(UpdateController::CheckMode::Manual);
    service.fail(UpdateService::Error::Network);
    controller.checkForUpdates(UpdateController::CheckMode::Manual);

    QCOMPARE(service.fetchCount, 2);
}

void UpdateControllerTest::exposesAvailableReleaseAndOpensIt()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    FakeUpdateService service;
    UpdateController controller(settings, service, QStringLiteral("0.1.0"));
    const UpdateRelease release {
        .version = QStringLiteral("0.2.0"),
        .releasePageUrl = QUrl(QStringLiteral("https://example.com/release")),
        .downloadUrl = QUrl(QStringLiteral("https://example.com/update.dmg")),
    };

    controller.checkForUpdates(UpdateController::CheckMode::Manual);
    service.succeed(release);

    QVERIFY(controller.updateAvailable());
    QVERIFY(controller.directDownloadAvailable());
    QCOMPARE(controller.latestVersion(), QStringLiteral("0.2.0"));
    QCOMPARE(controller.status(), UpdateController::Status::UpdateAvailable);
    QVERIFY(controller.openDownloadPage());
    QCOMPARE(service.openCount, 1);
    QCOMPARE(service.openedRelease.downloadUrl, release.downloadUrl);
}

void UpdateControllerTest::doesNotOfferCurrentOrInvalidRelease()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    FakeUpdateService service;
    UpdateController controller(settings, service, QStringLiteral("0.2.0"));

    controller.checkForUpdates(UpdateController::CheckMode::Manual);
    service.succeed(UpdateRelease {
        .version = QStringLiteral("0.2.0"),
        .releasePageUrl = QUrl(QStringLiteral("https://example.com/release")),
    });
    QVERIFY(!controller.updateAvailable());

    controller.checkForUpdates(UpdateController::CheckMode::Manual);
    service.succeed(UpdateRelease {
        .version = QStringLiteral("nightly"),
        .releasePageUrl = QUrl(QStringLiteral("https://example.com/release")),
    });
    QVERIFY(!controller.updateAvailable());
}

QTEST_MAIN(UpdateControllerTest)
#include "test_updatecontroller.moc"
