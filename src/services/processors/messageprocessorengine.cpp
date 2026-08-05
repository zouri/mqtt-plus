#include "messageprocessorengine.h"

#include "services/processors/processorruntimeadapter.h"
#include "services/processors/processorruntimeregistry.h"
#include "services/processors/processorvaluecodec.h"

#include <QCborArray>
#include <QCborMap>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

namespace {

constexpr auto kInvalidSource = "invalid_source";
constexpr auto kRuntimeUnavailable = "runtime_unavailable";
constexpr auto kContractUnsupported = "contract_unsupported";
constexpr auto kPreparationFailed = "preparation_failed";
constexpr auto kExecutionFailed = "execution_failed";
constexpr auto kExecutionTimedOut = "execution_timed_out";
constexpr auto kExecutionCancelled = "execution_cancelled";
constexpr auto kUnsupportedResult = "unsupported_result";
constexpr auto kResultLimitExceeded = "result_limit_exceeded";
constexpr auto kDiagnosticsTruncated = "diagnostics_truncated";
constexpr auto kInternalRuntimeError = "internal_runtime_error";

ProcessorDiagnostic diagnostic(const char *code, const QString &message)
{
    ProcessorDiagnostic result;
    result.code = QString::fromLatin1(code);
    result.message = message;
    return result;
}

ProcessorExecutionLimits normalizedLimits(const ProcessorExecutionLimits &source)
{
    ProcessorExecutionLimits limits = source;
    limits.wallTimeMilliseconds = std::clamp(
        limits.wallTimeMilliseconds,
        1,
        ProcessorExecutionLimits::kMaximumWallTimeMilliseconds);
    limits.maxResultBytes = (std::max)(qsizetype(0), limits.maxResultBytes);
    limits.maxDiagnosticsBytes = (std::max)(qsizetype(0), limits.maxDiagnosticsBytes);
    limits.maxResultDepth = (std::max)(0, limits.maxResultDepth);
    limits.maxCollectionEntries = (std::max)(qsizetype(0), limits.maxCollectionEntries);
    limits.maxPreviewCharacters = (std::max)(0, limits.maxPreviewCharacters);
    return limits;
}

bool isStableIdentifier(const QString &value)
{
    return !value.isEmpty() && value == value.trimmed() && !value.contains(QChar::Null);
}

ProcessorValidationResult validateRevisionStructure(
    const ProcessorRevisionSnapshot &revision)
{
    auto failure = [](const QString &message) {
        ProcessorValidationResult result;
        result.state = ProcessorValidationState::InvalidSource;
        result.diagnostics.append(diagnostic(kInvalidSource, message));
        return result;
    };

    if (!isStableIdentifier(revision.id)
        || !isStableIdentifier(revision.processorId)
        || revision.revisionNumber <= 0) {
        return failure(QStringLiteral("Processor revision identity is invalid."));
    }
    if (!isStableIdentifier(revision.contractId)
        || !isStableIdentifier(revision.languageId)
        || !isStableIdentifier(revision.runtimeId)) {
        return failure(QStringLiteral("Processor contract, language, or runtime identity is invalid."));
    }
    if (!isStableIdentifier(revision.contentHash)) {
        return failure(QStringLiteral("Processor revision content hash is invalid."));
    }
    if (revision.entryFile.isEmpty() || revision.entrySymbol.trimmed().isEmpty()) {
        return failure(QStringLiteral("Processor revision entry point is invalid."));
    }
    if (revision.files.isEmpty()) {
        return failure(QStringLiteral("Processor revision contains no source files."));
    }

    bool foundEntryFile = false;
    QSet<QString> foldedPaths;
    for (const ProcessorSourceFile &file : revision.files) {
        if (file.path.isEmpty()) {
            return failure(QStringLiteral("Processor revision contains an empty source path."));
        }
        const QString foldedPath = file.path.toCaseFolded();
        if (foldedPaths.contains(foldedPath)) {
            return failure(QStringLiteral("Processor revision contains duplicate source paths."));
        }
        foldedPaths.insert(foldedPath);
        foundEntryFile = foundEntryFile || file.path == revision.entryFile;
    }
    if (!foundEntryFile) {
        return failure(QStringLiteral("Processor entry file is not part of the revision."));
    }

    ProcessorValidationResult result;
    result.state = ProcessorValidationState::Ready;
    return result;
}

QString normalizedDiagnosticCode(const QString &source)
{
    if (source.isEmpty() || source.size() > 64) {
        return QString::fromLatin1(kInternalRuntimeError);
    }
    for (const QChar character : source) {
        const ushort value = character.unicode();
        const bool valid = (value >= 'a' && value <= 'z')
            || (value >= '0' && value <= '9')
            || value == '_';
        if (!valid) {
            return QString::fromLatin1(kInternalRuntimeError);
        }
    }
    return source;
}

QString truncateUtf8(const QString &source, qsizetype maxBytes)
{
    if (maxBytes <= 0) {
        return {};
    }
    if (source.toUtf8().size() <= maxBytes) {
        return source;
    }

    qsizetype low = 0;
    qsizetype high = source.size();
    while (low < high) {
        const qsizetype middle = low + (high - low + 1) / 2;
        QString candidate = source.left(middle);
        if (!candidate.isEmpty()) {
            const ushort last = candidate.back().unicode();
            if (last >= 0xd800 && last <= 0xdbff) {
                candidate.chop(1);
            }
        }
        if (candidate.toUtf8().size() <= maxBytes) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }

    QString result = source.left(low);
    if (!result.isEmpty()) {
        const ushort last = result.back().unicode();
        if (last >= 0xd800 && last <= 0xdbff) {
            result.chop(1);
        }
    }
    while (!result.isEmpty() && result.toUtf8().size() > maxBytes) {
        result.chop(1);
    }
    return result;
}

QString truncateCharacters(const QString &source, int maxCharacters)
{
    QString result = source.left(maxCharacters);
    if (!result.isEmpty()) {
        const ushort last = result.back().unicode();
        if (last >= 0xd800 && last <= 0xdbff) {
            result.chop(1);
        }
    }
    return result;
}

qsizetype diagnosticSize(const ProcessorDiagnostic &diagnostic)
{
    return diagnostic.code.toUtf8().size()
        + diagnostic.message.toUtf8().size()
        + diagnostic.file.toUtf8().size();
}

QVector<ProcessorDiagnostic> boundedDiagnostics(
    const QVector<ProcessorDiagnostic> &source,
    qsizetype maxBytes)
{
    QVector<ProcessorDiagnostic> normalized;
    normalized.reserve(source.size());
    qsizetype totalBytes = 0;
    bool exceedsLimit = false;
    for (ProcessorDiagnostic item : source) {
        item.code = normalizedDiagnosticCode(item.code);
        const qsizetype itemBytes = diagnosticSize(item);
        if (itemBytes > maxBytes - totalBytes) {
            exceedsLimit = true;
            totalBytes = maxBytes;
        } else {
            totalBytes += itemBytes;
        }
        normalized.append(std::move(item));
    }
    if (!exceedsLimit && totalBytes <= maxBytes) {
        return normalized;
    }
    if (maxBytes <= 0) {
        return {};
    }

    ProcessorDiagnostic truncated = diagnostic(
        kDiagnosticsTruncated,
        QStringLiteral("Processor diagnostics were truncated."));
    const qsizetype truncatedBytes = diagnosticSize(truncated);
    const qsizetype contentBudget = maxBytes >= truncatedBytes
        ? maxBytes - truncatedBytes
        : maxBytes;

    QVector<ProcessorDiagnostic> result;
    qsizetype usedBytes = 0;
    for (ProcessorDiagnostic item : normalized) {
        if (usedBytes >= contentBudget) {
            break;
        }
        const qsizetype codeBytes = item.code.toUtf8().size();
        if (codeBytes > contentBudget - usedBytes) {
            break;
        }
        usedBytes += codeBytes;

        const qsizetype fileBudget = (std::min)(
            qsizetype(1024),
            (contentBudget - usedBytes) / 4);
        item.file = truncateUtf8(item.file, fileBudget);
        usedBytes += item.file.toUtf8().size();

        item.message = truncateUtf8(item.message, contentBudget - usedBytes);
        usedBytes += item.message.toUtf8().size();
        result.append(std::move(item));
    }
    if (truncatedBytes <= maxBytes - usedBytes) {
        result.append(std::move(truncated));
    }
    return result;
}

void ensureDiagnosticCode(
    QVector<ProcessorDiagnostic> &diagnostics,
    const char *code,
    const QString &message)
{
    if (!code) {
        return;
    }
    const QString expected = QString::fromLatin1(code);
    const auto it = std::find_if(
        diagnostics.begin(),
        diagnostics.end(),
        [&expected](const ProcessorDiagnostic &item) {
            return item.code == expected;
        });
    if (it == diagnostics.end()) {
        diagnostics.prepend(diagnostic(code, message));
    } else if (it != diagnostics.begin()) {
        ProcessorDiagnostic item = std::move(*it);
        diagnostics.erase(it);
        diagnostics.prepend(std::move(item));
    }
}

const char *defaultDiagnosticCode(ProcessorValidationState state)
{
    switch (state) {
    case ProcessorValidationState::Ready:
        return nullptr;
    case ProcessorValidationState::InvalidSource:
        return kInvalidSource;
    case ProcessorValidationState::RuntimeUnavailable:
        return kRuntimeUnavailable;
    case ProcessorValidationState::PreparationFailed:
        return kPreparationFailed;
    case ProcessorValidationState::InternalError:
        return kInternalRuntimeError;
    }
    return kInternalRuntimeError;
}

const char *defaultDiagnosticCode(ProcessorExecutionState state)
{
    switch (state) {
    case ProcessorExecutionState::Succeeded:
        return nullptr;
    case ProcessorExecutionState::InvalidSource:
        return kInvalidSource;
    case ProcessorExecutionState::RuntimeUnavailable:
        return kRuntimeUnavailable;
    case ProcessorExecutionState::PreparationFailed:
        return kPreparationFailed;
    case ProcessorExecutionState::ExecutionFailed:
        return kExecutionFailed;
    case ProcessorExecutionState::TimedOut:
        return kExecutionTimedOut;
    case ProcessorExecutionState::Cancelled:
        return kExecutionCancelled;
    case ProcessorExecutionState::OutputLimitExceeded:
        return kResultLimitExceeded;
    case ProcessorExecutionState::UnsupportedResult:
        return kUnsupportedResult;
    case ProcessorExecutionState::InternalError:
        return kInternalRuntimeError;
    }
    return kInternalRuntimeError;
}

ProcessorExecutionState executionState(ProcessorValidationState state)
{
    switch (state) {
    case ProcessorValidationState::Ready:
        return ProcessorExecutionState::Succeeded;
    case ProcessorValidationState::InvalidSource:
        return ProcessorExecutionState::InvalidSource;
    case ProcessorValidationState::RuntimeUnavailable:
        return ProcessorExecutionState::RuntimeUnavailable;
    case ProcessorValidationState::PreparationFailed:
        return ProcessorExecutionState::PreparationFailed;
    case ProcessorValidationState::InternalError:
        return ProcessorExecutionState::InternalError;
    }
    return ProcessorExecutionState::InternalError;
}

enum class ValueIssue
{
    None,
    Unsupported,
    LimitExceeded,
};

struct ValueValidation
{
    ValueIssue issue = ValueIssue::None;
    QString message;
};

ValueValidation validateValue(
    const QCborValue &value,
    const ProcessorExecutionLimits &limits,
    int depth,
    qsizetype &entryCount)
{
    if (depth > limits.maxResultDepth) {
        return {
            ValueIssue::LimitExceeded,
            QStringLiteral("Processor result nesting exceeds the configured limit."),
        };
    }

    switch (value.type()) {
    case QCborValue::Null:
    case QCborValue::False:
    case QCborValue::True:
    case QCborValue::Integer:
    case QCborValue::ByteArray:
    case QCborValue::String:
        return {};
    case QCborValue::Double:
        return std::isfinite(value.toDouble())
            ? ValueValidation {}
            : ValueValidation {
                  ValueIssue::Unsupported,
                  QStringLiteral("Processor result contains a non-finite number."),
              };
    case QCborValue::Array: {
        const QCborArray array = value.toArray();
        if (array.size() > limits.maxCollectionEntries - entryCount) {
            return {
                ValueIssue::LimitExceeded,
                QStringLiteral("Processor result contains too many collection entries."),
            };
        }
        entryCount += array.size();
        for (const QCborValue &item : array) {
            const ValueValidation itemResult = validateValue(
                item,
                limits,
                depth + 1,
                entryCount);
            if (itemResult.issue != ValueIssue::None) {
                return itemResult;
            }
        }
        return {};
    }
    case QCborValue::Map: {
        const QCborMap map = value.toMap();
        if (map.size() > limits.maxCollectionEntries - entryCount) {
            return {
                ValueIssue::LimitExceeded,
                QStringLiteral("Processor result contains too many collection entries."),
            };
        }
        entryCount += map.size();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            if (!it.key().isString()) {
                return {
                    ValueIssue::Unsupported,
                    QStringLiteral("Processor result map keys must be strings."),
                };
            }
            const ValueValidation itemResult = validateValue(
                it.value(),
                limits,
                depth + 1,
                entryCount);
            if (itemResult.issue != ValueIssue::None) {
                return itemResult;
            }
        }
        return {};
    }
    default:
        return {
            ValueIssue::Unsupported,
            QStringLiteral("Processor result contains an unsupported value type."),
        };
    }
}

void addLengthPrefixed(QCryptographicHash &hash, const QByteArray &value)
{
    QByteArray lengthBytes(sizeof(quint64), Qt::Uninitialized);
    qToBigEndian<quint64>(
        static_cast<quint64>(value.size()),
        reinterpret_cast<uchar *>(lengthBytes.data()));
    hash.addData(lengthBytes);
    hash.addData(value);
}

QByteArray preparedCacheKey(
    const RuntimeDescriptor &descriptor,
    const ProcessorRevisionSnapshot &revision)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    addLengthPrefixed(hash, QByteArrayLiteral("mqtt-plus-prepared-processor-v1"));
    addLengthPrefixed(hash, descriptor.runtimeId.toUtf8());
    addLengthPrefixed(hash, descriptor.runtimeVersion.toUtf8());
    addLengthPrefixed(hash, revision.languageId.toUtf8());
    addLengthPrefixed(hash, revision.contractId.toUtf8());
    addLengthPrefixed(hash, revision.contentHash.toUtf8());
    return hash.result();
}

struct PreparedResolution
{
    ProcessorValidationState state = ProcessorValidationState::InternalError;
    QSharedPointer<ProcessorRuntimeAdapter> adapter;
    PreparedProcessorHandle prepared;
    QVector<ProcessorDiagnostic> diagnostics;
};

} // namespace

class MessageProcessorEngine::Private
{
public:
    explicit Private(
        const QVector<QSharedPointer<ProcessorRuntimeAdapter>> &adapters,
        int sourceMaxCacheEntries)
        : maxCacheEntries((std::max)(1, sourceMaxCacheEntries))
    {
        for (const QSharedPointer<ProcessorRuntimeAdapter> &adapter : adapters) {
            try {
                QString error;
                if (!registry.addAdapter(adapter, &error)) {
                    configurationError = error;
                    break;
                }
            } catch (const std::exception &exception) {
                configurationError = QStringLiteral(
                    "Processor runtime registration raised an exception: %1")
                                         .arg(QString::fromUtf8(exception.what()));
                break;
            } catch (...) {
                configurationError = QStringLiteral(
                    "Processor runtime registration raised an unknown exception.");
                break;
            }
        }
    }

    PreparedResolution prepare(
        const ProcessorRevisionSnapshot &revision,
        const ProcessorExecutionLimits &limits)
    {
        if (!configurationError.isEmpty()) {
            return {
                ProcessorValidationState::InternalError,
                {},
                {},
                {diagnostic(kInternalRuntimeError, configurationError)},
            };
        }

        const ProcessorValidationResult structure = validateRevisionStructure(revision);
        if (!structure.isReady()) {
            return {
                structure.state,
                {},
                {},
                structure.diagnostics,
            };
        }

        const QSharedPointer<ProcessorRuntimeAdapter> adapter = registry.adapter(revision.runtimeId);
        const std::optional<RuntimeDescriptor> descriptor = registry.descriptor(
            revision.runtimeId);
        if (adapter.isNull() || !descriptor) {
            return {
                ProcessorValidationState::RuntimeUnavailable,
                {},
                {},
                {diagnostic(
                    kRuntimeUnavailable,
                    QStringLiteral("Processor runtime is not installed: %1")
                        .arg(revision.runtimeId))},
            };
        }
        if (descriptor->languageId != revision.languageId) {
            return {
                ProcessorValidationState::RuntimeUnavailable,
                {},
                {},
                {diagnostic(
                    kRuntimeUnavailable,
                    QStringLiteral("Processor runtime %1 does not support language %2.")
                        .arg(revision.runtimeId, revision.languageId))},
            };
        }
        if (!descriptor->supportedContractIds.contains(revision.contractId)) {
            return {
                ProcessorValidationState::RuntimeUnavailable,
                {},
                {},
                {diagnostic(
                    kContractUnsupported,
                    QStringLiteral("Processor runtime %1 does not support contract %2.")
                        .arg(revision.runtimeId, revision.contractId))},
            };
        }

        const QByteArray cacheKey = preparedCacheKey(*descriptor, revision);
        auto cached = preparedCache.find(cacheKey);
        if (cached != preparedCache.end()) {
            cached->lastUse = ++cacheClock;
            return {
                ProcessorValidationState::Ready,
                adapter,
                cached->prepared,
                cached->diagnostics,
            };
        }

        ProcessorPreparationResult preparation;
        try {
            preparation = adapter->prepare(revision, limits);
        } catch (const std::exception &exception) {
            preparation.state = ProcessorValidationState::InternalError;
            preparation.diagnostics.append(diagnostic(
                kInternalRuntimeError,
                QStringLiteral("Processor preparation raised an exception: %1")
                    .arg(QString::fromUtf8(exception.what()))));
        } catch (...) {
            preparation.state = ProcessorValidationState::InternalError;
            preparation.diagnostics.append(diagnostic(
                kInternalRuntimeError,
                QStringLiteral("Processor preparation raised an unknown exception.")));
        }

        if (preparation.state != ProcessorValidationState::Ready) {
            ensureDiagnosticCode(
                preparation.diagnostics,
                defaultDiagnosticCode(preparation.state),
                QStringLiteral("Processor preparation failed."));
            return {
                preparation.state,
                adapter,
                {},
                preparation.diagnostics,
            };
        }
        if (preparation.prepared.isNull()) {
            return {
                ProcessorValidationState::InternalError,
                adapter,
                {},
                {diagnostic(
                    kInternalRuntimeError,
                    QStringLiteral("Processor runtime returned no prepared processor."))},
            };
        }

        CacheEntry cacheEntry;
        cacheEntry.prepared = preparation.prepared;
        cacheEntry.diagnostics = preparation.diagnostics;
        cacheEntry.lastUse = ++cacheClock;
        preparedCache.insert(cacheKey, cacheEntry);
        evictCacheEntries();
        return {
            ProcessorValidationState::Ready,
            adapter,
            preparation.prepared,
            preparation.diagnostics,
        };
    }

private:
    struct CacheEntry
    {
        PreparedProcessorHandle prepared;
        QVector<ProcessorDiagnostic> diagnostics;
        quint64 lastUse = 0;
    };

    void evictCacheEntries()
    {
        while (preparedCache.size() > maxCacheEntries) {
            auto oldest = preparedCache.end();
            for (auto it = preparedCache.begin(); it != preparedCache.end(); ++it) {
                if (oldest == preparedCache.end() || it->lastUse < oldest->lastUse) {
                    oldest = it;
                }
            }
            if (oldest == preparedCache.end()) {
                return;
            }
            preparedCache.erase(oldest);
        }
    }

    ProcessorRuntimeRegistry registry;
    QString configurationError;
    int maxCacheEntries = 32;
    quint64 cacheClock = 0;
    QHash<QByteArray, CacheEntry> preparedCache;
};

MessageProcessorEngine::MessageProcessorEngine(
    QVector<QSharedPointer<ProcessorRuntimeAdapter>> adapters,
    int maxPreparedCacheEntries)
    : m_private(std::make_unique<Private>(
          adapters,
          maxPreparedCacheEntries))
{
}

MessageProcessorEngine::~MessageProcessorEngine() = default;

ProcessorValidationResult MessageProcessorEngine::validate(
    const ProcessorRevisionSnapshot &revision,
    const ProcessorExecutionLimits &sourceLimits)
{
    const ProcessorExecutionLimits limits = normalizedLimits(sourceLimits);
    const PreparedResolution prepared = m_private->prepare(revision, limits);

    ProcessorValidationResult result;
    result.state = prepared.state;
    result.diagnostics = boundedDiagnostics(
        prepared.diagnostics,
        limits.maxDiagnosticsBytes);
    return result;
}

ProcessorExecutionResult MessageProcessorEngine::execute(
    const ProcessorRevisionSnapshot &revision,
    const MessageProcessorContext &context,
    const ProcessorExecutionLimits &sourceLimits)
{
    const ProcessorExecutionLimits limits = normalizedLimits(sourceLimits);
    const PreparedResolution prepared = m_private->prepare(revision, limits);
    if (prepared.state != ProcessorValidationState::Ready) {
        ProcessorExecutionResult result;
        result.state = executionState(prepared.state);
        result.diagnostics = boundedDiagnostics(
            prepared.diagnostics,
            limits.maxDiagnosticsBytes);
        return result;
    }

    ProcessorExecutionResult result;
    QElapsedTimer timer;
    timer.start();
    try {
        result = prepared.adapter->execute(prepared.prepared, context, limits);
    } catch (const std::exception &exception) {
        result.state = ProcessorExecutionState::InternalError;
        result.diagnostics.append(diagnostic(
            kInternalRuntimeError,
            QStringLiteral("Processor execution raised an exception: %1")
                .arg(QString::fromUtf8(exception.what()))));
    } catch (...) {
        result.state = ProcessorExecutionState::InternalError;
        result.diagnostics.append(diagnostic(
            kInternalRuntimeError,
            QStringLiteral("Processor execution raised an unknown exception.")));
    }
    result.durationMicroseconds = timer.nsecsElapsed() / 1000;
    result.diagnostics = prepared.diagnostics + result.diagnostics;

    if (result.state == ProcessorExecutionState::Succeeded) {
        qsizetype entryCount = 0;
        const ValueValidation valueValidation = validateValue(
            result.value,
            limits,
            0,
            entryCount);
        if (valueValidation.issue != ValueIssue::None) {
            result.state = valueValidation.issue == ValueIssue::LimitExceeded
                ? ProcessorExecutionState::OutputLimitExceeded
                : ProcessorExecutionState::UnsupportedResult;
            result.value = {};
            result.preview.clear();
            result.diagnostics.append(diagnostic(
                valueValidation.issue == ValueIssue::LimitExceeded
                    ? kResultLimitExceeded
                    : kUnsupportedResult,
                valueValidation.message));
        } else {
            result.value = ProcessorValueCodec::canonicalize(result.value);
            const QByteArray encoded = result.value.toCbor();
            if (encoded.size() > limits.maxResultBytes) {
                result.state = ProcessorExecutionState::OutputLimitExceeded;
                result.value = {};
                result.preview.clear();
                result.diagnostics.append(diagnostic(
                    kResultLimitExceeded,
                    QStringLiteral("Processor result exceeds the configured byte limit.")));
            } else {
                result.preview = truncateCharacters(
                    result.value.toDiagnosticNotation(QCborValue::Compact),
                    limits.maxPreviewCharacters);
            }
        }
    } else {
        result.value = {};
        result.preview.clear();
    }

    if (result.state != ProcessorExecutionState::Succeeded) {
        ensureDiagnosticCode(
            result.diagnostics,
            defaultDiagnosticCode(result.state),
            QStringLiteral("Processor execution failed."));
    }

    result.diagnostics = boundedDiagnostics(
        result.diagnostics,
        limits.maxDiagnosticsBytes);
    return result;
}
