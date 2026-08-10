#include "usecases/updatecontroller.h"

#include <QDateTime>
#include <QTimer>
#include <QVersionNumber>

namespace {

constexpr auto kAutomaticChecksKey = "updates/automaticChecksEnabled";
constexpr auto kLastCheckAttemptKey = "updates/lastCheckAttemptUtc";
constexpr qint64 kAutomaticCheckIntervalSeconds = 24 * 60 * 60;

} // namespace

UpdateController::UpdateController(
    QSettings &settings,
    UpdateService &service,
    const QString &currentVersion,
    QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_service(service)
    , m_currentVersion(currentVersion)
    , m_automaticChecksEnabled(
          m_settings.value(QString::fromLatin1(kAutomaticChecksKey), true).toBool())
{
    connect(
        &m_service,
        &UpdateService::releaseFetched,
        this,
        &UpdateController::handleReleaseFetched);
    connect(
        &m_service,
        &UpdateService::fetchFailed,
        this,
        &UpdateController::handleFetchFailed);
}

QString UpdateController::currentVersion() const { return m_currentVersion; }

QString UpdateController::latestVersion() const { return m_latestRelease.version; }

UpdateController::Status UpdateController::status() const { return m_status; }

bool UpdateController::busy() const { return m_busy; }

bool UpdateController::updateAvailable() const { return m_updateAvailable; }

bool UpdateController::directDownloadAvailable() const
{
    return m_latestRelease.downloadUrl.isValid();
}

bool UpdateController::automaticChecksEnabled() const
{
    return m_automaticChecksEnabled;
}

void UpdateController::setAutomaticChecksEnabled(bool enabled)
{
    if (m_automaticChecksEnabled == enabled) {
        return;
    }
    m_automaticChecksEnabled = enabled;
    m_settings.setValue(QString::fromLatin1(kAutomaticChecksKey), enabled);
    emit automaticChecksEnabledChanged();
    if (enabled) {
        scheduleAutomaticCheck();
    }
}

void UpdateController::scheduleAutomaticCheck()
{
    if (!m_automaticChecksEnabled || !automaticCheckDue()) {
        return;
    }

    QTimer::singleShot(5000, this, [this]() {
        checkForUpdates(CheckMode::Automatic);
    });
}

void UpdateController::checkForUpdates(CheckMode mode)
{
    if (m_busy
        || (mode == CheckMode::Automatic
            && (!m_automaticChecksEnabled || !automaticCheckDue()))) {
        return;
    }

    m_settings.setValue(
        QString::fromLatin1(kLastCheckAttemptKey),
        QDateTime::currentDateTimeUtc());
    m_busy = true;
    m_userInitiated = mode == CheckMode::Manual;
    m_status = Status::Checking;
    emit stateChanged();
    m_service.fetchLatestRelease();
}

bool UpdateController::openDownloadPage() const
{
    return m_updateAvailable && m_service.openRelease(m_latestRelease);
}

void UpdateController::handleReleaseFetched(const UpdateRelease &release)
{
    if (!m_busy) {
        return;
    }

    m_latestRelease = release;
    const QVersionNumber releaseVersion = QVersionNumber::fromString(release.version);
    const QVersionNumber currentVersion = QVersionNumber::fromString(m_currentVersion);
    m_updateAvailable = !releaseVersion.isNull()
        && !currentVersion.isNull()
        && QVersionNumber::compare(releaseVersion, currentVersion) > 0;
    m_busy = false;
    m_status = m_updateAvailable ? Status::UpdateAvailable : Status::UpToDate;
    const bool userInitiated = m_userInitiated;
    emit stateChanged();
    emit checkCompleted(m_updateAvailable, userInitiated);
}

void UpdateController::handleFetchFailed(UpdateService::Error error)
{
    if (!m_busy) {
        return;
    }

    switch (error) {
    case UpdateService::Error::NoReleases:
        m_status = Status::NoReleases;
        break;
    case UpdateService::Error::Network:
        m_status = Status::NetworkError;
        break;
    case UpdateService::Error::InvalidRelease:
        m_status = Status::InvalidRelease;
        break;
    }
    m_busy = false;
    const bool userInitiated = m_userInitiated;
    emit stateChanged();
    emit checkFailed(error, userInitiated);
}

bool UpdateController::automaticCheckDue() const
{
    const QDateTime lastAttempt = m_settings
                                      .value(QString::fromLatin1(kLastCheckAttemptKey))
                                      .toDateTime()
                                      .toUTC();
    return !lastAttempt.isValid()
        || lastAttempt.secsTo(QDateTime::currentDateTimeUtc())
            >= kAutomaticCheckIntervalSeconds;
}
