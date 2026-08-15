#include "services/storage/draftstore.h"

#include "domain/sessionconfig.h"
#include "services/payload/payloadcodec.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QMqttTopicName>

namespace {
constexpr int kSchemaVersion = 3;
constexpr int kMinimumReadableSchemaVersion = 2;
constexpr qsizetype kMaxNameLength = 80;
constexpr qsizetype kMaxDescriptionLength = 500;
constexpr qsizetype kMaxPayloadBytes = 16 * 1024 * 1024;

bool storedPayloadExceedsLimit(const QString &payload)
{
    return payload.size() > kMaxPayloadBytes
        || payload.toUtf8().size() > kMaxPayloadBytes;
}

bool validateDrafts(const QVector<PublishDraft> &drafts, QString &errorMessage)
{
    QSet<QString> ids;
    QSet<QString> normalizedNames;
    for (const PublishDraft &draft : drafts) {
        const QString id = draft.id.trimmed();
        const QString name = draft.name.trimmed();
        if (id.isEmpty() || ids.contains(id)) {
            errorMessage = QStringLiteral("Draft library contains a missing or duplicate ID.");
            return false;
        }
        if (name.isEmpty() || name.size() > kMaxNameLength) {
            errorMessage = QStringLiteral("Draft library contains an invalid name.");
            return false;
        }
        const QString normalizedName = name.toCaseFolded();
        if (normalizedNames.contains(normalizedName)) {
            errorMessage = QStringLiteral("Draft library contains duplicate names.");
            return false;
        }
        if (draft.description.size() > kMaxDescriptionLength) {
            errorMessage = QStringLiteral("Draft library contains an overlong description.");
            return false;
        }
        const QString topic = draft.defaultTopic.trimmed();
        if (!topic.isEmpty() && !QMqttTopicName(topic).isValid()) {
            errorMessage = QStringLiteral("Draft library contains an invalid default topic.");
            return false;
        }
        bool formatOk = false;
        const PayloadFormat format = PayloadCodec::formatFromId(draft.formatId, &formatOk);
        if (!formatOk) {
            errorMessage = QStringLiteral("Draft library contains an unsupported payload format.");
            return false;
        }
        if (storedPayloadExceedsLimit(draft.payload)) {
            errorMessage = QStringLiteral("Draft library contains an oversized payload.");
            return false;
        }
        QByteArray encodedPayload;
        QString payloadError;
        if (!PayloadCodec::encodeForPublish(format, draft.payload, encodedPayload, payloadError)
            || encodedPayload.size() > kMaxPayloadBytes) {
            errorMessage = payloadError.isEmpty()
                ? QStringLiteral("Draft library contains an oversized payload.")
                : payloadError;
            return false;
        }
        if (draft.qos < 0 || draft.qos > SessionConfig::kMaximumQos) {
            errorMessage = QStringLiteral("Draft library contains an unsupported QoS value.");
            return false;
        }
        ids.insert(id);
        normalizedNames.insert(normalizedName);
    }
    return true;
}

QByteArray readFile(const QString &path, QString &errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = file.errorString();
        return {};
    }
    return file.readAll();
}

bool writeFile(const QString &path, const QByteArray &content, QString &errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMessage = file.errorString();
        return false;
    }
    if (file.write(content) != content.size()) {
        errorMessage = file.errorString();
        return false;
    }
    if (!file.commit()) {
        errorMessage = file.errorString();
        return false;
    }
    QFile::setPermissions(
        path,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

DraftStore::LoadResult parseDocument(const QByteArray &content)
{
    DraftStore::LoadResult result;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.state = DraftStore::LoadState::Corrupt;
        result.errorMessage = QStringLiteral("Cannot parse drafts.json: %1").arg(parseError.errorString());
        return result;
    }

    const QJsonObject root = document.object();
    const int version = root.value(QStringLiteral("version")).toInt(-1);
    if (version > kSchemaVersion) {
        result.state = DraftStore::LoadState::Incompatible;
        result.errorMessage = QStringLiteral("Draft library version %1 requires a newer application.").arg(version);
        return result;
    }
    if (version < kMinimumReadableSchemaVersion
        || !root.value(QStringLiteral("drafts")).isArray()) {
        result.state = DraftStore::LoadState::Corrupt;
        result.errorMessage = QStringLiteral("Unsupported or incomplete draft library schema.");
        return result;
    }

    const QJsonArray rows = root.value(QStringLiteral("drafts")).toArray();
    result.drafts.reserve(rows.size());
    for (const QJsonValue &value : rows) {
        if (!value.isObject()) {
            result.state = DraftStore::LoadState::Corrupt;
            result.errorMessage = QStringLiteral("Draft library contains a non-object record.");
            result.drafts.clear();
            return result;
        }
        const QJsonObject row = value.toObject();
        PublishDraft draft;
        draft.id = row.value(QStringLiteral("id")).toString();
        draft.name = row.value(QStringLiteral("name")).toString();
        draft.description = row.value(QStringLiteral("description")).toString();
        draft.defaultTopic = row.value(QStringLiteral("defaultTopic")).toString();
        draft.payload = row.value(QStringLiteral("payload")).toString();
        draft.formatId = row.value(QStringLiteral("format")).toString();
        draft.qos = row.value(QStringLiteral("qos")).toInt(-1);
        draft.retain = row.value(QStringLiteral("retain")).toBool(false);
        if (version >= 3) {
            const auto properties = mqttPublishPropertiesFromBase64Cbor(
                row.value(QStringLiteral("propertiesCborBase64")).toString());
            if (properties) {
                draft.properties = *properties;
            }
        }
        draft.createdAt = row.value(QStringLiteral("createdAt")).toString();
        draft.updatedAt = row.value(QStringLiteral("updatedAt")).toString();
        draft.lastUsedAt = row.value(QStringLiteral("lastUsedAt")).toString();
        if (draft.id.trimmed().isEmpty() || draft.name.trimmed().isEmpty()
            || draft.formatId.trimmed().isEmpty() || draft.qos < 0) {
            result.state = DraftStore::LoadState::Corrupt;
            result.errorMessage = QStringLiteral("Draft library contains an incomplete record.");
            result.drafts.clear();
            return result;
        }
        result.drafts.append(draft);
    }
    if (!validateDrafts(result.drafts, result.errorMessage)) {
        result.state = DraftStore::LoadState::Corrupt;
        result.drafts.clear();
    }
    return result;
}

QByteArray serializeDrafts(const QVector<PublishDraft> &drafts)
{
    QJsonArray rows;
    for (const PublishDraft &draft : drafts) {
        QJsonObject row;
        row.insert(QStringLiteral("id"), draft.id);
        row.insert(QStringLiteral("name"), draft.name);
        row.insert(QStringLiteral("description"), draft.description);
        row.insert(QStringLiteral("defaultTopic"), draft.defaultTopic);
        row.insert(QStringLiteral("payload"), draft.payload);
        row.insert(QStringLiteral("format"), draft.formatId);
        row.insert(QStringLiteral("qos"), draft.qos);
        row.insert(QStringLiteral("retain"), draft.retain);
        row.insert(
            QStringLiteral("propertiesCborBase64"),
            mqttPublishPropertiesToBase64Cbor(draft.properties));
        row.insert(QStringLiteral("createdAt"), draft.createdAt);
        row.insert(QStringLiteral("updatedAt"), draft.updatedAt);
        row.insert(QStringLiteral("lastUsedAt"), draft.lastUsedAt);
        rows.append(row);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), kSchemaVersion);
    root.insert(QStringLiteral("drafts"), rows);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

}

namespace DraftStore {

QString storageDirectory(const QString &overrideRoot)
{
    if (!overrideRoot.trimmed().isEmpty()) {
        return QDir(overrideRoot).absolutePath();
    }
    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString basePath = configRoot.isEmpty()
        ? QDir::home().filePath(QStringLiteral(".config"))
        : configRoot;
    return QDir(basePath).filePath(QStringLiteral("mqtt_plus/drafts"));
}

QString primaryFilePath(const QString &overrideRoot)
{
    return QDir(storageDirectory(overrideRoot)).filePath(QStringLiteral("drafts.json"));
}

QString backupFilePath(const QString &overrideRoot)
{
    return QDir(storageDirectory(overrideRoot)).filePath(QStringLiteral("drafts.json.bak"));
}

LoadResult loadDrafts(const QString &overrideRoot)
{
    const QString primaryPath = primaryFilePath(overrideRoot);
    if (!QFileInfo::exists(primaryPath)) {
        const QString backupPath = backupFilePath(overrideRoot);
        if (!QFileInfo::exists(backupPath)) {
            return {};
        }

        QString backupError;
        const LoadResult backupResult = parseDocument(readFile(backupPath, backupError));
        LoadResult result;
        result.state = LoadState::Corrupt;
        result.canRecover = backupError.isEmpty() && backupResult.state == LoadState::Ready;
        result.errorMessage = result.canRecover
            ? QStringLiteral("The draft library file is missing. Restore the available backup.")
            : QStringLiteral("The draft library file is missing and its backup is unavailable or invalid.");
        return result;
    }

    QString errorMessage;
    const QByteArray content = readFile(primaryPath, errorMessage);
    if (!errorMessage.isEmpty()) {
        LoadResult result;
        result.state = LoadState::Corrupt;
        result.errorMessage = QStringLiteral("Cannot read %1: %2").arg(primaryPath, errorMessage);
        result.canRecover = QFileInfo::exists(backupFilePath(overrideRoot));
        return result;
    }

    LoadResult result = parseDocument(content);
    if (result.state == LoadState::Corrupt) {
        const QString backupPath = backupFilePath(overrideRoot);
        if (QFileInfo::exists(backupPath)) {
            QString backupError;
            const LoadResult backupResult = parseDocument(readFile(backupPath, backupError));
            result.canRecover = backupError.isEmpty() && backupResult.state == LoadState::Ready;
        }
    }
    return result;
}

SaveResult saveDrafts(const QVector<PublishDraft> &drafts, const QString &overrideRoot)
{
    SaveResult result;
    if (!validateDrafts(drafts, result.errorMessage)) {
        result.errorMessage = QStringLiteral("Refusing to save an invalid draft library: %1")
                                  .arg(result.errorMessage);
        return result;
    }
    QDir dir;
    const QString directory = storageDirectory(overrideRoot);
    if (!dir.mkpath(directory)) {
        result.errorMessage = QStringLiteral("Cannot create draft storage directory: %1").arg(directory);
        return result;
    }

    const QString primaryPath = primaryFilePath(overrideRoot);
    if (QFileInfo::exists(primaryPath)) {
        QString readError;
        const QByteArray previousContent = readFile(primaryPath, readError);
        if (!readError.isEmpty() || parseDocument(previousContent).state != LoadState::Ready) {
            result.errorMessage = QStringLiteral("Refusing to overwrite an unreadable draft library.");
            return result;
        }
        if (!writeFile(backupFilePath(overrideRoot), previousContent, result.errorMessage)) {
            result.errorMessage = QStringLiteral("Cannot update draft backup: %1").arg(result.errorMessage);
            return result;
        }
    }

    if (!writeFile(primaryPath, serializeDrafts(drafts), result.errorMessage)) {
        result.errorMessage = QStringLiteral("Cannot save draft library: %1").arg(result.errorMessage);
        return result;
    }
    result.ok = true;
    return result;
}

SaveResult recoverBackup(const QString &overrideRoot)
{
    SaveResult result;
    const QString backupPath = backupFilePath(overrideRoot);
    QString readError;
    const QByteArray backupContent = readFile(backupPath, readError);
    if (!readError.isEmpty() || parseDocument(backupContent).state != LoadState::Ready) {
        result.errorMessage = QStringLiteral("The draft backup is unavailable or invalid.");
        return result;
    }

    const QString primaryPath = primaryFilePath(overrideRoot);
    if (QFileInfo::exists(primaryPath)) {
        QString primaryReadError;
        const QByteArray primaryContent = readFile(primaryPath, primaryReadError);
        if (primaryReadError.isEmpty()
            && parseDocument(primaryContent).state == LoadState::Incompatible) {
            result.errorMessage = QStringLiteral("Refusing to replace a newer draft library version.");
            return result;
        }
        const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
        const QString corruptPath = QDir(storageDirectory(overrideRoot))
                                        .filePath(QStringLiteral("drafts.json.corrupt-%1").arg(stamp));
        if (!QFile::rename(primaryPath, corruptPath)) {
            result.errorMessage = QStringLiteral("Cannot preserve the damaged draft library before recovery.");
            return result;
        }
    }

    if (!writeFile(primaryPath, backupContent, result.errorMessage)) {
        result.errorMessage = QStringLiteral("Cannot restore the draft backup: %1").arg(result.errorMessage);
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace DraftStore
