#pragma once

#include "domain/configurationbundle.h"
#include "domain/sessionconfig.h"

#include <QFutureWatcher>
#include <QObject>
#include <QStringList>
#include <QUrl>

class DraftLibraryService;
class PreferencesController;
class QSettings;
class SessionService;

class ConfigurationTransferService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool hasPreview READ hasPreview NOTIFY previewChanged)
    Q_PROPERTY(QString previewFormat READ previewFormat NOTIFY previewChanged)
    Q_PROPERTY(QString previewFileName READ previewFileName NOTIFY previewChanged)
    Q_PROPERTY(int previewConnectionCount READ previewConnectionCount NOTIFY previewChanged)
    Q_PROPERTY(int previewSubscriptionCount READ previewSubscriptionCount NOTIFY previewChanged)
    Q_PROPERTY(int previewDraftCount READ previewDraftCount NOTIFY previewChanged)
    Q_PROPERTY(bool previewContainsSensitiveData READ previewContainsSensitiveData NOTIFY previewChanged)
    Q_PROPERTY(QStringList previewWarnings READ previewWarnings NOTIFY previewChanged)

public:
    explicit ConfigurationTransferService(
        SessionService &sessionService,
        DraftLibraryService &draftService,
        PreferencesController &preferences,
        QSettings &settings,
        QString importedCertificateRoot = {},
        QObject *parent = nullptr);

    bool busy() const;
    bool hasPreview() const;
    QString previewFormat() const;
    QString previewFileName() const;
    int previewConnectionCount() const;
    int previewSubscriptionCount() const;
    int previewDraftCount() const;
    bool previewContainsSensitiveData() const;
    QStringList previewWarnings() const;

    Q_INVOKABLE bool inspectImportFile(const QUrl &fileUrl);
    Q_INVOKABLE void importPreview(bool includeSensitiveData);
    Q_INVOKABLE bool exportConfiguration(
        const QUrl &fileUrl,
        bool includeSensitiveData);
    Q_INVOKABLE void clearPreview();

signals:
    void busyChanged();
    void previewChanged();
    void importPreviewReady();
    void portableSettingsImported(bool logRetentionLimitChanged);
    void operationFinished(bool success, const QString &title, const QString &message);

private:
    void finishImportInspection();
    ConfigurationTransfer::Bundle exportBundle(
        bool includeSensitiveData,
        QStringList &warnings) const;
    QVariantMap portableSettings() const;
    bool applyPortableSettings(const QVariantMap &settings, QString &errorMessage);
    bool prepareImport(
        ConfigurationTransfer::Bundle bundle,
        bool includeSensitiveData,
        QString &errorMessage);
    bool materializeSessionAssets(
        ConfigurationTransfer::SessionData &session,
        const QString &sessionId,
        SessionConnectionConfig &config,
        QString &errorMessage);
    void finishSuccessfulImport();
    void rollbackImport(const QString &message);
    void resetPendingImport();
    void setBusy(bool busy);
    QString importSummary() const;

    SessionService &m_sessionService;
    DraftLibraryService &m_draftService;
    PreferencesController &m_preferences;
    QSettings &m_settings;
    QString m_importedCertificateRoot;
    QFutureWatcher<ConfigurationTransfer::ParseResult> m_inspectWatcher;
    ConfigurationTransfer::ParseResult m_preview;
    QString m_pendingPreviewFileName;
    QString m_previewFileName;
    QVariantMap m_previousPreferences;
    QStringList m_importedSessionIds;
    QStringList m_createdAssetFiles;
    QStringList m_createdAssetDirectories;
    QVector<PublishDraft> m_pendingDrafts;
    bool m_busy = false;
    bool m_preferencesApplied = false;
    bool m_logRetentionLimitChanged = false;
    bool m_waitingForDrafts = false;
    int m_importedConnectionCount = 0;
    int m_importedSubscriptionCount = 0;
    int m_importedDraftCount = 0;
};
