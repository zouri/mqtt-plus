#pragma once

#include "domain/updaterelease.h"
#include "services/update/updateservice.h"

#include <QObject>
#include <QSettings>
#include <QString>

class UpdateController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool directDownloadAvailable READ directDownloadAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool automaticChecksEnabled READ automaticChecksEnabled WRITE setAutomaticChecksEnabled NOTIFY automaticChecksEnabledChanged)

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
    QString statusMessage() const;
    Status status() const;
    bool busy() const;
    bool updateAvailable() const;
    bool directDownloadAvailable() const;
    bool automaticChecksEnabled() const;

    void setAutomaticChecksEnabled(bool enabled);
    void scheduleAutomaticCheck();
    void checkForUpdates(CheckMode mode);

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE bool openDownloadPage() const;

public slots:
    void retranslate();

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
