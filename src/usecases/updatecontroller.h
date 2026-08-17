#pragma once

#include "domain/updaterelease.h"
#include "services/update/updateservice.h"

#include <QObject>
#include <QSettings>
#include <QString>

class UpdateController : public QObject
{
    Q_OBJECT

public:
    enum class CheckMode
    {
        Manual,
        Automatic,
    };

    enum class Status
    {
        Idle,
        Checking,
        UpdateAvailable,
        UpToDate,
        NoReleases,
        NetworkError,
        InvalidRelease,
    };

    explicit UpdateController(
        QSettings &settings,
        UpdateService &service,
        const QString &currentVersion,
        QObject *parent = nullptr);

    QString currentVersion() const;
    QString latestVersion() const;
    Status status() const;
    bool busy() const;
    bool updateAvailable() const;
    bool directDownloadAvailable() const;
    bool automaticChecksEnabled() const;

    void setAutomaticChecksEnabled(bool enabled);
    void scheduleAutomaticCheck();
    void checkForUpdates(CheckMode mode);
    bool openDownloadPage() const;

signals:
    void stateChanged();
    void automaticChecksEnabledChanged();
    void checkCompleted(bool updateAvailable, bool userInitiated);
    void checkFailed(UpdateService::Error error, bool userInitiated);

private:
    void handleReleaseFetched(const UpdateRelease &release);
    void handleFetchFailed(UpdateService::Error error);
    bool automaticCheckDue() const;

    QSettings &m_settings;
    UpdateService &m_service;
    QString m_currentVersion;
    UpdateRelease m_latestRelease;
    Status m_status = Status::Idle;
    bool m_busy = false;
    bool m_updateAvailable = false;
    bool m_userInitiated = false;
    bool m_automaticChecksEnabled = true;
};
