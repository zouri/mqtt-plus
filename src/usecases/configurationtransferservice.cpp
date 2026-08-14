#include "usecases/configurationtransferservice.h"

#include "domain/sessionconfig.h"
#include "services/configuration/configurationadapters.h"
#include "usecases/draftlibraryservice.h"
#include "usecases/preferencescontroller.h"
#include "usecases/sessionservice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <QtConcurrentRun>

#include <QMqttClient>

#include <algorithm>
#include <utility>

namespace {
constexpr qint64 kMaxImportFileBytes = 64 * 1024 * 1024;
constexpr qsizetype kMaxDraftNameLength = 80;

QString text(const char *source)
{
    return QCoreApplication::translate("ConfigurationTransferService", source);
}

QString localPath(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : QString();
}

QByteArray readFile(const QString &path, QString &errorMessage)
{
    const QFileInfo info(path);
    if (!info.isFile()) {
        errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationTransferService",
            "Select a regular configuration file."));
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = file.errorString();
        return {};
    }
    if (file.size() > kMaxImportFileBytes) {
        errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationTransferService",
            "The selected configuration file is larger than 64 MiB."));
        return {};
    }

    QByteArray content;
    content.reserve(static_cast<qsizetype>((std::min)(file.size(), kMaxImportFileBytes)));
    while (!file.atEnd()) {
        const qint64 remaining = kMaxImportFileBytes - content.size();
        const QByteArray chunk = file.read((std::min)(qint64(64 * 1024), remaining + 1));
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            errorMessage = file.errorString();
            return {};
        }
        content.append(chunk);
        if (content.size() > kMaxImportFileBytes) {
            errorMessage = text(QT_TRANSLATE_NOOP(
                "ConfigurationTransferService",
                "The selected configuration file is larger than 64 MiB."));
            return {};
        }
    }
    return content;
}

bool writePrivateFile(const QString &path, const QByteArray &content, QString &errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        errorMessage = file.errorString();
        return false;
    }
    if (file.write(content) != content.size()) {
        errorMessage = file.errorString();
        return false;
    }
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationTransferService",
            "Cannot restrict permissions for the private file."));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        errorMessage = file.errorString();
        return false;
    }
    return true;
}

bool startsWithJsonArray(const QByteArray &content)
{
    for (const char value : content) {
        if (value == ' ' || value == '\t' || value == '\r' || value == '\n') {
            continue;
        }
        return value == '[';
    }
    return false;
}

ConfigurationTransfer::ParseResult inspectFile(const QString &path)
{
    QString errorMessage;
    const QByteArray content = readFile(path, errorMessage);
    if (!errorMessage.isEmpty()) {
        ConfigurationTransfer::ParseResult result;
        result.errorMessage = errorMessage;
        return result;
    }

    ConfigurationTransfer::ParseResult result = startsWithJsonArray(content)
        ? MqttxConfigAdapter::parse(content)
        : MqttPlusConfigAdapter::parse(content);
    if (!result.ok) {
        return result;
    }
    return result;
}

QString uniqueName(
    const QString &sourceName,
    QSet<QString> &normalizedNames,
    const QString &fallback,
    qsizetype maximumLength = -1)
{
    QString base = sourceName.trimmed();
    if (base.isEmpty()) {
        base = fallback;
    }
    if (maximumLength > 0) {
        base = base.left(maximumLength);
    }
    QString candidate = base;
    int suffix = 2;
    while (normalizedNames.contains(candidate.toCaseFolded())) {
        const QString marker = text(QT_TRANSLATE_NOOP(
            "ConfigurationTransferService",
            " (Imported %1)")).arg(suffix++);
        candidate = maximumLength > 0
            ? base.left(std::max<qsizetype>(1, maximumLength - marker.size())) + marker
            : base + marker;
    }
    normalizedNames.insert(candidate.toCaseFolded());
    return candidate;
}

QByteArray readOptionalAsset(const QString &path, QStringList &warnings)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        warnings.append(text(QT_TRANSLATE_NOOP(
                                 "ConfigurationTransferService",
                                 "A TLS certificate file could not be included: %1"))
                            .arg(path));
        return {};
    }
    return file.readAll();
}

} // namespace

ConfigurationTransferService::ConfigurationTransferService(
    SessionService &sessionService,
    DraftLibraryService &draftService,
    PreferencesController &preferences,
    QSettings &settings,
    QString importedCertificateRoot,
    QObject *parent)
    : QObject(parent)
    , m_sessionService(sessionService)
    , m_draftService(draftService)
    , m_preferences(preferences)
    , m_settings(settings)
    , m_importedCertificateRoot(std::move(importedCertificateRoot))
{
    connect(
        &m_inspectWatcher,
        &QFutureWatcher<ConfigurationTransfer::ParseResult>::finished,
        this,
        &ConfigurationTransferService::finishImportInspection);
    connect(
        &m_draftService,
        &DraftLibraryService::operationSucceeded,
        this,
        [this](const QString &operation, const QString &) {
            if (m_waitingForDrafts && operation == QStringLiteral("import")) {
                finishSuccessfulImport();
            }
        });
    connect(
        &m_draftService,
        &DraftLibraryService::storageError,
        this,
        [this](const QString &message) {
            if (m_waitingForDrafts) {
                rollbackImport(
                    message.isEmpty()
                        ? text(QT_TRANSLATE_NOOP(
                              "ConfigurationTransferService",
                              "Cannot import drafts."))
                        : message);
            }
        });
}

bool ConfigurationTransferService::busy() const { return m_busy; }
bool ConfigurationTransferService::hasPreview() const { return m_preview.ok; }
QString ConfigurationTransferService::previewFormat() const { return m_preview.format; }
QString ConfigurationTransferService::previewFileName() const { return m_previewFileName; }
int ConfigurationTransferService::previewConnectionCount() const { return m_preview.bundle.sessions.size(); }
int ConfigurationTransferService::previewDraftCount() const { return m_preview.bundle.drafts.size(); }
bool ConfigurationTransferService::previewContainsSensitiveData() const { return m_preview.sensitiveFieldCount > 0; }
QStringList ConfigurationTransferService::previewWarnings() const { return m_preview.warnings; }

int ConfigurationTransferService::previewSubscriptionCount() const
{
    int count = 0;
    for (const auto &session : m_preview.bundle.sessions) {
        count += session.subscriptions.size();
    }
    return count;
}

bool ConfigurationTransferService::inspectImportFile(const QUrl &fileUrl)
{
    if (m_busy) {
        return false;
    }
    clearPreview();
    const QString path = localPath(fileUrl);
    if (path.isEmpty()) {
        emit operationFinished(
            false,
            text(QT_TRANSLATE_NOOP("ConfigurationTransferService", "Import failed")),
            text(QT_TRANSLATE_NOOP(
                "ConfigurationTransferService",
                "Select a local configuration file.")));
        return false;
    }

    m_pendingPreviewFileName = QFileInfo(path).fileName();
    setBusy(true);
    m_inspectWatcher.setFuture(QtConcurrent::run([path]() { return inspectFile(path); }));
    return true;
}

void ConfigurationTransferService::finishImportInspection()
{
    ConfigurationTransfer::ParseResult result = m_inspectWatcher.result();
    const QString fileName = std::exchange(m_pendingPreviewFileName, QString());
    if (!result.ok) {
        const QString message = result.errorMessage.isEmpty()
            ? text(QT_TRANSLATE_NOOP(
                  "ConfigurationTransferService",
                  "The configuration file is not supported."))
            : result.errorMessage;
        m_preview = {};
        m_previewFileName.clear();
        setBusy(false);
        emit previewChanged();
        emit operationFinished(
            false,
            text(QT_TRANSLATE_NOOP("ConfigurationTransferService", "Import failed")),
            message);
        return;
    }

    m_preview = std::move(result);
    m_previewFileName = fileName;
    setBusy(false);
    emit previewChanged();
    emit importPreviewReady();
}

void ConfigurationTransferService::importPreview(bool includeSensitiveData)
{
    if (m_busy || !m_preview.ok) {
        return;
    }
    setBusy(true);
    QString errorMessage;
    if (!prepareImport(m_preview.bundle, includeSensitiveData, errorMessage)) {
        rollbackImport(
            errorMessage.isEmpty()
                ? text(QT_TRANSLATE_NOOP(
                      "ConfigurationTransferService",
                      "The configuration could not be imported."))
                : errorMessage);
    }
}

bool ConfigurationTransferService::exportConfiguration(
    const QUrl &fileUrl,
    bool includeSensitiveData)
{
    if (m_busy) {
        return false;
    }
    QString path = localPath(fileUrl);
    if (path.isEmpty()) {
        emit operationFinished(
            false,
            text(QT_TRANSLATE_NOOP("ConfigurationTransferService", "Export failed")),
            text(QT_TRANSLATE_NOOP(
                "ConfigurationTransferService",
                "Select a local destination file.")));
        return false;
    }
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".mqttplus.json");
    }

    QStringList warnings;
    ConfigurationTransfer::Bundle bundle = exportBundle(includeSensitiveData, warnings);
    const ConfigurationTransfer::SerializeResult serialized =
        MqttPlusConfigAdapter::serialize(bundle, includeSensitiveData);
    if (!serialized.ok) {
        emit operationFinished(
            false,
            text(QT_TRANSLATE_NOOP("ConfigurationTransferService", "Export failed")),
            serialized.errorMessage);
        return false;
    }

    QString errorMessage;
    if (!writePrivateFile(path, serialized.content, errorMessage)) {
        emit operationFinished(
            false,
            text(QT_TRANSLATE_NOOP("ConfigurationTransferService", "Export failed")),
            errorMessage);
        return false;
    }
    QString message = text(QT_TRANSLATE_NOOP(
                               "ConfigurationTransferService",
                               "Configuration exported to %1."))
                          .arg(QFileInfo(path).fileName());
    warnings.append(serialized.warnings);
    warnings.removeDuplicates();
    if (!warnings.isEmpty()) {
        message += QStringLiteral(" ") + warnings.join(QStringLiteral(" "));
    }
    emit operationFinished(
        true,
        text(QT_TRANSLATE_NOOP("ConfigurationTransferService", "Configuration exported")),
        message);
    return true;
}

void ConfigurationTransferService::clearPreview()
{
    if (m_busy) {
        return;
    }
    m_preview = {};
    m_pendingPreviewFileName.clear();
    m_previewFileName.clear();
    emit previewChanged();
}

ConfigurationTransfer::Bundle ConfigurationTransferService::exportBundle(
    bool includeSensitiveData,
    QStringList &warnings) const
{
    using namespace ConfigurationTransfer;

    Bundle bundle;
    bundle.sessions.reserve(m_sessionService.sessions().size());
    for (const SessionState &state : m_sessionService.sessions()) {
        const QMqttClient *client = state.runtime.client;
        SessionData session;
        session.sourceId = state.id;
        session.name = state.name;
        session.host = client ? client->hostname() : QString();
        session.port = client ? client->port() : 1883;
        session.transport = state.transport;
        session.protocolVersion = state.protocolVersion;
        session.sslSecure = state.sslSecure;
        session.alpn = state.alpn;
        session.certificateType = state.certificateType;
        session.caCertificate = readOptionalAsset(state.caFile, warnings);
        session.clientCertificate = readOptionalAsset(state.clientCertificateFile, warnings);
        if (includeSensitiveData) {
            session.clientKey = readOptionalAsset(state.clientKeyFile, warnings);
        }
        session.clientId = client ? client->clientId() : QString();
        session.username = client ? client->username() : QString();
        session.password = includeSensitiveData && client ? client->password() : QString();
        session.cleanSession = client ? client->cleanSession() : true;
        session.keepAliveSeconds = client ? client->keepAlive() : SessionConfig::kDefaultKeepAlive;
        session.connectTimeoutSeconds = state.connectTimeoutSeconds;
        session.sessionExpiryInterval = state.sessionExpiryInterval;
        session.receiveMaximum = state.receiveMaximum;
        session.maximumPacketSize = state.maximumPacketSize;
        session.topicAliasMaximum = state.topicAliasMaximum;
        session.requestResponseInformation = state.requestResponseInformation;
        session.requestProblemInformation = state.requestProblemInformation;
        session.authenticationMethod = state.authenticationMethod;
        session.authenticationData = includeSensitiveData ? state.authenticationData : QString();
        session.outputPaused = state.outputPaused;
        session.capturePolicy = state.capturePolicy;
        session.subscriptions.reserve(state.subscriptions.size());
        for (const SubscriptionEntry &entry : state.subscriptions) {
            SubscriptionData subscription;
            subscription.topic = entry.topic;
            subscription.alias = entry.alias;
            subscription.qos = entry.requestedQos;
            subscription.format = entry.format;
            subscription.color = entry.color;
            subscription.paused = entry.paused;
            session.subscriptions.append(subscription);
        }
        bundle.sessions.append(std::move(session));
    }
    bundle.drafts = m_draftService.drafts();
    bundle.preferences = portableSettings();
    return bundle;
}

QVariantMap ConfigurationTransferService::portableSettings() const
{
    QVariantMap settings = m_preferences.portableSettings();
    const QVariantMap defaults {
        {QStringLiteral("appearance/themeMode"), QStringLiteral("system")},
        {QStringLiteral("appearance/themeColor"), QStringLiteral("mint")},
        {QStringLiteral("appearance/animationsEnabled"), true},
        {QStringLiteral("appearance/languageMode"), QStringLiteral("system")},
        {QStringLiteral("workbench/messagePayloadDisplayMode"), QStringLiteral("hover")},
    };
    for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
        settings.insert(it.key(), m_settings.value(it.key(), it.value()));
    }
    return settings;
}

bool ConfigurationTransferService::applyPortableSettings(
    const QVariantMap &settings,
    QString &errorMessage)
{
    errorMessage.clear();
    const QStringList appearanceKeys {
        QStringLiteral("appearance/themeMode"),
        QStringLiteral("appearance/themeColor"),
        QStringLiteral("appearance/animationsEnabled"),
        QStringLiteral("appearance/languageMode"),
        QStringLiteral("workbench/messagePayloadDisplayMode"),
    };
    for (const QString &key : appearanceKeys) {
        if (settings.contains(key)) {
            m_settings.setValue(key, settings.value(key));
        }
    }
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError) {
        errorMessage = m_settings.status() == QSettings::AccessError
            ? text(QT_TRANSLATE_NOOP(
                  "ConfigurationTransferService",
                  "Cannot write imported settings: access denied."))
            : text(QT_TRANSLATE_NOOP(
                  "ConfigurationTransferService",
                  "Cannot write imported settings: invalid settings format."));
        return false;
    }
    return m_preferences.applyPortableSettings(settings, errorMessage);
}

bool ConfigurationTransferService::prepareImport(
    ConfigurationTransfer::Bundle bundle,
    bool includeSensitiveData,
    QString &errorMessage)
{
    using namespace ConfigurationTransfer;

    if (!bundle.drafts.isEmpty()
        && (!m_draftService.ready() || m_draftService.readOnly() || m_draftService.busy())) {
        errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationTransferService",
            "The Draft Library is not ready for import."));
        return false;
    }

    m_previousPreferences = portableSettings();
    m_importedConnectionCount = bundle.sessions.size();
    m_importedDraftCount = bundle.drafts.size();
    m_importedSubscriptionCount = 0;

    QSet<QString> draftNames;
    QSet<QString> draftIds;
    for (const PublishDraft &draft : m_draftService.drafts()) {
        draftNames.insert(draft.name.trimmed().toCaseFolded());
        draftIds.insert(draft.id);
    }
    m_pendingDrafts.clear();
    for (PublishDraft draft : std::as_const(bundle.drafts)) {
        draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        while (draftIds.contains(draft.id)) {
            draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        draftIds.insert(draft.id);
        draft.name = uniqueName(
            draft.name,
            draftNames,
            text(QT_TRANSLATE_NOOP("ConfigurationTransferService", "Imported draft")),
            kMaxDraftNameLength);
        m_pendingDrafts.append(std::move(draft));
    }

    QVector<SessionImportRequest> sessionRequests;
    sessionRequests.reserve(bundle.sessions.size());
    for (SessionData session : std::as_const(bundle.sessions)) {
        if (!includeSensitiveData) {
            session.password.clear();
            session.authenticationData.clear();
            session.clientKey.clear();
        }
        SessionImportRequest request;
        request.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        SessionConnectionConfig config;
        config.name = session.name;
        config.host = session.host;
        config.port = session.port;
        config.transport = session.transport;
        config.protocolVersion = session.protocolVersion;
        config.sslSecure = session.sslSecure;
        config.alpn = session.alpn;
        config.certificateType = session.certificateType;
        config.clientId = session.clientId;
        config.username = session.username;
        config.password = session.password;
        config.cleanSession = session.cleanSession;
        config.keepAliveSeconds = session.keepAliveSeconds;
        config.connectTimeoutSeconds = session.connectTimeoutSeconds;
        config.sessionExpiryInterval = session.sessionExpiryInterval;
        config.receiveMaximum = session.receiveMaximum;
        config.maximumPacketSize = session.maximumPacketSize;
        config.topicAliasMaximum = session.topicAliasMaximum;
        config.requestResponseInformation = session.requestResponseInformation;
        config.requestProblemInformation = session.requestProblemInformation;
        config.authenticationMethod = session.authenticationMethod;
        config.authenticationData = session.authenticationData;
        if (!materializeSessionAssets(session, request.id, config, errorMessage)) {
            return false;
        }
        request.config = config;
        request.outputPaused = session.outputPaused;
        request.capturePolicy = session.capturePolicy;
        request.subscriptions.reserve(session.subscriptions.size());
        for (const SubscriptionData &source : session.subscriptions) {
            SubscriptionEntry entry;
            entry.topic = source.topic;
            entry.alias = source.alias;
            entry.requestedQos = source.qos;
            entry.format = source.format;
            entry.color = source.color;
            entry.paused = source.paused;
            request.subscriptions.append(entry);
            ++m_importedSubscriptionCount;
        }
        sessionRequests.append(std::move(request));
    }

    if (!m_sessionService.importSessions(
            sessionRequests,
            m_importedSessionIds,
            errorMessage)) {
        return false;
    }

    if (!bundle.preferences.isEmpty()) {
        const int previousLogRetentionLimit = m_preferences.logRetentionLimit();
        m_preferencesApplied = true;
        if (!applyPortableSettings(bundle.preferences, errorMessage)) {
            return false;
        }
        m_logRetentionLimitChanged =
            bundle.preferences.contains(QStringLiteral("history/logRetentionLimit"))
            && m_preferences.logRetentionLimit() != previousLogRetentionLimit;
    }

    if (!m_pendingDrafts.isEmpty()) {
        m_waitingForDrafts = true;
        if (!m_draftService.importDrafts(m_pendingDrafts)) {
            m_waitingForDrafts = false;
            errorMessage = m_draftService.errorMessage().isEmpty()
                ? text(QT_TRANSLATE_NOOP(
                      "ConfigurationTransferService",
                      "Cannot save imported drafts."))
                : m_draftService.errorMessage();
            return false;
        }
        return true;
    }

    finishSuccessfulImport();
    return true;
}

bool ConfigurationTransferService::materializeSessionAssets(
    ConfigurationTransfer::SessionData &session,
    const QString &sessionId,
    SessionConnectionConfig &config,
    QString &errorMessage)
{
    const bool hasAssets = !session.caCertificate.isEmpty()
        || !session.clientCertificate.isEmpty()
        || !session.clientKey.isEmpty();
    if (!hasAssets) {
        return true;
    }
    QString certificateRoot = m_importedCertificateRoot;
    if (certificateRoot.isEmpty()) {
        const QString configRoot = QStandardPaths::writableLocation(
            QStandardPaths::GenericConfigLocation);
        const QString baseRoot = configRoot.isEmpty()
            ? QDir::homePath() + QStringLiteral("/.config")
            : configRoot;
        certificateRoot = QDir(baseRoot).filePath(
            QStringLiteral("mqtt_plus/imported-certificates"));
    }
    const QString directory = QDir(certificateRoot).filePath(sessionId);
    if (QFileInfo::exists(directory)) {
        errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationTransferService",
            "The imported certificate directory already exists."));
        return false;
    }
    if (!QDir().mkpath(directory)) {
        errorMessage = text(QT_TRANSLATE_NOOP(
            "ConfigurationTransferService",
            "Cannot create the imported certificate directory."));
        return false;
    }
    m_createdAssetDirectories.append(directory);

    const auto writeAsset = [this, &directory, &errorMessage](
                                const QByteArray &content,
                                const QString &fileName,
                                QString &path) {
        if (content.isEmpty()) {
            return true;
        }
        path = QDir(directory).filePath(fileName);
        if (!writePrivateFile(path, content, errorMessage)) {
            return false;
        }
        m_createdAssetFiles.append(path);
        return true;
    };

    QString caPath;
    QString certificatePath;
    QString keyPath;
    if (!writeAsset(session.caCertificate, QStringLiteral("ca.pem"), caPath)
        || !writeAsset(session.clientCertificate, QStringLiteral("client.pem"), certificatePath)
        || !writeAsset(session.clientKey, QStringLiteral("client-key.pem"), keyPath)) {
        return false;
    }
    config.caFile = caPath;
    config.clientCertificateFile = certificatePath;
    config.clientKeyFile = keyPath;
    return true;
}

void ConfigurationTransferService::finishSuccessfulImport()
{
    const QString message = importSummary();
    const bool preferencesApplied = m_preferencesApplied;
    const bool logRetentionLimitChanged = m_logRetentionLimitChanged;
    m_preview = {};
    m_previewFileName.clear();
    resetPendingImport();
    setBusy(false);
    emit previewChanged();
    if (preferencesApplied) {
        emit portableSettingsImported(logRetentionLimitChanged);
    }
    emit operationFinished(
        true,
        text(QT_TRANSLATE_NOOP("ConfigurationTransferService", "Configuration imported")),
        message);
}

void ConfigurationTransferService::rollbackImport(const QString &message)
{
    QStringList rollbackErrors;
    bool sessionsRolledBack = true;
    m_waitingForDrafts = false;
    if (m_preferencesApplied) {
        QString error;
        if (!applyPortableSettings(m_previousPreferences, error)) {
            rollbackErrors.append(error);
        }
    }
    if (!m_importedSessionIds.isEmpty()) {
        QString error;
        if (!m_sessionService.rollbackImportedSessions(m_importedSessionIds, error)) {
            sessionsRolledBack = false;
            rollbackErrors.append(error);
        }
    }
    if (sessionsRolledBack) {
        for (const QString &path : std::as_const(m_createdAssetFiles)) {
            if (QFileInfo::exists(path) && !QFile::remove(path)) {
                rollbackErrors.append(
                    text(QT_TRANSLATE_NOOP(
                             "ConfigurationTransferService",
                             "Cannot remove an imported certificate file: %1"))
                        .arg(path));
            }
        }
        for (const QString &path : std::as_const(m_createdAssetDirectories)) {
            QDir directory(path);
            if (directory.exists() && !directory.removeRecursively()) {
                rollbackErrors.append(
                    text(QT_TRANSLATE_NOOP(
                             "ConfigurationTransferService",
                             "Cannot remove an imported certificate directory: %1"))
                        .arg(path));
            }
        }
    }

    QString fullMessage = message;
    rollbackErrors.removeAll(QString());
    if (!rollbackErrors.isEmpty()) {
        fullMessage += QStringLiteral(" ")
            + text(QT_TRANSLATE_NOOP(
                       "ConfigurationTransferService",
                       "Rollback also reported: %1"))
                  .arg(rollbackErrors.join(QStringLiteral(" ")));
    }
    resetPendingImport();
    setBusy(false);
    emit operationFinished(
        false,
        text(QT_TRANSLATE_NOOP("ConfigurationTransferService", "Import failed")),
        fullMessage);
}

void ConfigurationTransferService::resetPendingImport()
{
    m_previousPreferences.clear();
    m_importedSessionIds.clear();
    m_createdAssetFiles.clear();
    m_createdAssetDirectories.clear();
    m_pendingDrafts.clear();
    m_preferencesApplied = false;
    m_logRetentionLimitChanged = false;
    m_waitingForDrafts = false;
    m_importedConnectionCount = 0;
    m_importedSubscriptionCount = 0;
    m_importedDraftCount = 0;
}

void ConfigurationTransferService::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

QString ConfigurationTransferService::importSummary() const
{
    return text(QT_TRANSLATE_NOOP(
                    "ConfigurationTransferService",
                    "Imported %1 connections, %2 subscriptions, and %3 drafts."))
        .arg(m_importedConnectionCount)
        .arg(m_importedSubscriptionCount)
        .arg(m_importedDraftCount);
}
