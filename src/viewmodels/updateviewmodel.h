#pragma once

#include <QObject>
#include <QString>

class UpdateController;

class UpdateViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY updateStateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY updateStateChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateStateChanged)
    Q_PROPERTY(bool directDownloadAvailable READ directDownloadAvailable NOTIFY updateStateChanged)
    Q_PROPERTY(bool automaticChecksEnabled READ automaticChecksEnabled WRITE setAutomaticChecksEnabled NOTIFY automaticChecksEnabledChanged)

public:
    explicit UpdateViewModel(
        UpdateController &controller,
        QObject *parent = nullptr);

    QString currentVersion() const;
    QString latestVersion() const;
    QString statusMessage() const;
    bool busy() const;
    bool updateAvailable() const;
    bool directDownloadAvailable() const;
    bool automaticChecksEnabled() const;

    void setAutomaticChecksEnabled(bool enabled);

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE bool openDownloadPage() const;

public slots:
    void retranslate();

signals:
    void updateStateChanged();
    void automaticChecksEnabledChanged();

private:
    void refreshStatusMessage();

    UpdateController &m_controller;
    QString m_statusMessage;
};
