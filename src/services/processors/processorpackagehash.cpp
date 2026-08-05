#include "processorpackagehash.h"

#include <QCborArray>
#include <QCborValue>
#include <QCryptographicHash>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

void addLengthPrefixed(QCryptographicHash &hash, const QByteArray &value)
{
    QByteArray lengthBytes(sizeof(quint64), Qt::Uninitialized);
    qToBigEndian<quint64>(
        static_cast<quint64>(value.size()),
        reinterpret_cast<uchar *>(lengthBytes.data()));
    hash.addData(lengthBytes);
    hash.addData(value);
}

QString normalizedPath(const QString &sourcePath, QString &error)
{
    QString path = sourcePath.normalized(QString::NormalizationForm_C);
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (path.isEmpty()) {
        error = QStringLiteral("Processor source path cannot be empty.");
        return {};
    }
    if (path.startsWith(QLatin1Char('/'))
        || QRegularExpression(QStringLiteral("^[A-Za-z]:")).match(path).hasMatch()
        || QRegularExpression(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*:")).match(path).hasMatch()) {
        error = QStringLiteral("Processor source path must be relative: %1").arg(sourcePath);
        return {};
    }

    const QStringList parts = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    for (const QString &part : parts) {
        if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral("..")) {
            error = QStringLiteral("Processor source path contains an invalid segment: %1").arg(sourcePath);
            return {};
        }
    }
    return parts.join(QLatin1Char('/'));
}

bool validateManifestValue(
    const QCborValue &value,
    int depth,
    int maxDepth,
    QString &error)
{
    if (depth > maxDepth) {
        error = QStringLiteral("Processor manifest nesting is too deep.");
        return false;
    }

    switch (value.type()) {
    case QCborValue::Null:
    case QCborValue::False:
    case QCborValue::True:
    case QCborValue::Integer:
    case QCborValue::String:
        return true;
    case QCborValue::Double:
        if (!std::isfinite(value.toDouble())) {
            error = QStringLiteral("Processor manifest numbers must be finite.");
            return false;
        }
        return true;
    case QCborValue::Array: {
        const QCborArray array = value.toArray();
        for (const QCborValue &item : array) {
            if (!validateManifestValue(item, depth + 1, maxDepth, error)) {
                return false;
            }
        }
        return true;
    }
    case QCborValue::Map: {
        const QCborMap map = value.toMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            if (!it.key().isString()) {
                error = QStringLiteral("Processor manifest map keys must be strings.");
                return false;
            }
            if (!validateManifestValue(it.value(), depth + 1, maxDepth, error)) {
                return false;
            }
        }
        return true;
    }
    default:
        error = QStringLiteral("Processor manifest contains an unsupported value type.");
        return false;
    }
}

QString requiredIdentifier(const QString &value, const QString &label, QString &error)
{
    const QString normalized = value.trimmed();
    if (normalized.isEmpty()) {
        error = QStringLiteral("%1 is required.").arg(label);
    }
    return normalized;
}

} // namespace

namespace ProcessorPackageHash {

PreparedProcessorPackage prepare(
    const ProcessorRevisionContent &sourceContent,
    const ProcessorPackageLimits &limits)
{
    PreparedProcessorPackage result;
    ProcessorRevisionContent content = sourceContent;

    content.contractId = requiredIdentifier(content.contractId, QStringLiteral("Processor contract ID"), result.error);
    if (!result.error.isEmpty()) {
        return result;
    }
    content.languageId = requiredIdentifier(content.languageId, QStringLiteral("Processor language ID"), result.error);
    if (!result.error.isEmpty()) {
        return result;
    }
    content.runtimeId = requiredIdentifier(content.runtimeId, QStringLiteral("Processor runtime ID"), result.error);
    if (!result.error.isEmpty()) {
        return result;
    }
    content.entrySymbol = requiredIdentifier(content.entrySymbol, QStringLiteral("Processor entry symbol"), result.error);
    if (!result.error.isEmpty()) {
        return result;
    }

    if (content.files.isEmpty()) {
        result.error = QStringLiteral("Processor revision must contain at least one source file.");
        return result;
    }
    if (limits.maxFiles <= 0 || content.files.size() > limits.maxFiles) {
        result.error = QStringLiteral("Processor revision contains too many source files.");
        return result;
    }

    QHash<QString, QString> normalizedPathByFoldedPath;
    qint64 totalBytes = 0;
    for (ProcessorSourceFile &file : content.files) {
        file.path = normalizedPath(file.path, result.error);
        if (!result.error.isEmpty()) {
            return result;
        }

        const QString foldedPath = file.path.toCaseFolded();
        if (normalizedPathByFoldedPath.contains(foldedPath)) {
            result.error = QStringLiteral("Processor revision contains duplicate source paths: %1 and %2")
                               .arg(normalizedPathByFoldedPath.value(foldedPath), file.path);
            return result;
        }
        normalizedPathByFoldedPath.insert(foldedPath, file.path);

        if (limits.maxFileBytes < 0 || file.content.size() > limits.maxFileBytes) {
            result.error = QStringLiteral("Processor source file exceeds the size limit: %1").arg(file.path);
            return result;
        }
        totalBytes += file.content.size();
        if (limits.maxTotalBytes < 0 || totalBytes > limits.maxTotalBytes) {
            result.error = QStringLiteral("Processor source package exceeds the total size limit.");
            return result;
        }

        if (file.mediaType.trimmed().isEmpty()) {
            file.mediaType = QStringLiteral("application/octet-stream");
        } else {
            file.mediaType = file.mediaType.trimmed();
        }
        file.contentHash = QString::fromLatin1(
            QCryptographicHash::hash(file.content, QCryptographicHash::Sha256).toHex());
    }

    content.entryFile = normalizedPath(content.entryFile, result.error);
    if (!result.error.isEmpty()) {
        return result;
    }
    if (!normalizedPathByFoldedPath.contains(content.entryFile.toCaseFolded())) {
        result.error = QStringLiteral("Processor entry file is not present in the source package: %1")
                           .arg(content.entryFile);
        return result;
    }
    content.entryFile = normalizedPathByFoldedPath.value(content.entryFile.toCaseFolded());

    QString manifestError;
    if (!validateManifestValue(
            QCborValue(content.manifest),
            0,
            (std::max)(0, limits.maxManifestDepth),
            manifestError)) {
        result.error = manifestError;
        return result;
    }

    const QJsonValue manifestValue = QCborValue(content.manifest).toJsonValue();
    if (!manifestValue.isObject()) {
        result.error = QStringLiteral("Processor manifest must be a map.");
        return result;
    }
    result.manifestJson = QJsonDocument(manifestValue.toObject()).toJson(QJsonDocument::Compact);
    if (limits.maxManifestBytes < 0 || result.manifestJson.size() > limits.maxManifestBytes) {
        result.error = QStringLiteral("Processor manifest exceeds the size limit.");
        return result;
    }

    std::sort(
        content.files.begin(),
        content.files.end(),
        [](const ProcessorSourceFile &left, const ProcessorSourceFile &right) {
            return left.path < right.path;
        });

    QCryptographicHash packageHash(QCryptographicHash::Sha256);
    addLengthPrefixed(packageHash, QByteArrayLiteral("mqtt-plus-processor-package-v1"));
    addLengthPrefixed(packageHash, content.contractId.toUtf8());
    addLengthPrefixed(packageHash, content.languageId.toUtf8());
    addLengthPrefixed(packageHash, content.runtimeId.toUtf8());
    addLengthPrefixed(packageHash, content.entryFile.toUtf8());
    addLengthPrefixed(packageHash, content.entrySymbol.toUtf8());
    addLengthPrefixed(packageHash, result.manifestJson);
    for (const ProcessorSourceFile &file : std::as_const(content.files)) {
        addLengthPrefixed(packageHash, file.path.toUtf8());
        addLengthPrefixed(packageHash, file.content);
    }

    result.ok = true;
    result.content = std::move(content);
    result.contentHash = QString::fromLatin1(packageHash.result().toHex());
    return result;
}

} // namespace ProcessorPackageHash
