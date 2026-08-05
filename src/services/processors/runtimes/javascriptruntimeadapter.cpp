#include "javascriptruntimeadapter.h"

#include <QCborArray>
#include <QCborMap>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QJSEngine>
#include <QJSValue>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

namespace {

constexpr double kMaximumSafeJavaScriptInteger = 9007199254740991.0;
constexpr qsizetype kEnvelopeExpansionFactor = 8;
constexpr qsizetype kEnvelopeSlackCharacters = 4096;

struct JavaScriptSource
{
    QString path;
    QString source;
};

class JavaScriptPreparedProcessor final : public PreparedProcessor
{
public:
    QVector<JavaScriptSource> sources;
    QString entrySymbol;
    QString entryFile;
};

class JavaScriptWatchdog
{
public:
    JavaScriptWatchdog(QJSEngine &engine, int wallTimeMilliseconds)
        : m_engine(engine)
        , m_thread([this, wallTimeMilliseconds]() {
            std::unique_lock lock(m_mutex);
            const bool stopped = m_condition.wait_for(
                lock,
                std::chrono::milliseconds(wallTimeMilliseconds),
                [this]() {
                    return m_stopped;
                });
            if (!stopped) {
                m_timedOut.store(true, std::memory_order_release);
                m_engine.setInterrupted(true);
            }
        })
    {
    }

    ~JavaScriptWatchdog()
    {
        stop();
    }

    Q_DISABLE_COPY_MOVE(JavaScriptWatchdog)

    void stop()
    {
        {
            const std::lock_guard lock(m_mutex);
            m_stopped = true;
        }
        m_condition.notify_one();
        if (m_thread.joinable()) {
            m_thread.join();
        }
        m_engine.setInterrupted(false);
    }

    bool timedOut() const
    {
        return m_timedOut.load(std::memory_order_acquire);
    }

private:
    QJSEngine &m_engine;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_stopped = false;
    std::atomic_bool m_timedOut = false;
    std::thread m_thread;
};

struct JavaScriptHelpers
{
    QJSValue makeBytes;
    QJSValue invoke;
    QJSValue normalize;
};

struct JavaScriptFailure
{
    QString message;
    QString file;
    QString stack;
    int line = -1;
    int column = -1;
};

enum class JavaScriptValueIssue
{
    None,
    Unsupported,
    LimitExceeded,
    InvalidEnvelope,
};

struct JavaScriptValueResult
{
    JavaScriptValueIssue issue = JavaScriptValueIssue::None;
    QCborValue value;
    QString message;
};

ProcessorDiagnostic diagnostic(
    const QString &code,
    const QString &message,
    const QString &file = {},
    int line = -1,
    int column = -1)
{
    ProcessorDiagnostic result;
    result.code = code;
    result.message = message;
    result.file = file;
    result.line = line;
    result.column = column;
    return result;
}

QString helperBootstrapSource()
{
    return QString::fromLatin1(R"JS(
(function() {
    "use strict"

    const reflectApply = Reflect.apply
    const reflectConstruct = Reflect.construct
    const reflectOwnKeys = Reflect.ownKeys
    const objectGetOwnPropertyDescriptor = Object.getOwnPropertyDescriptor
    const objectGetPrototypeOf = Object.getPrototypeOf
    const objectHasOwnProperty = Object.prototype.hasOwnProperty
    const objectIs = Object.is
    const objectPrototype = Object.prototype
    const arrayPrototype = Array.prototype
    const arrayPush = Array.prototype.push
    const arrayBufferPrototype = ArrayBuffer.prototype
    const uint8ArrayConstructor = Uint8Array
    const uint8ArrayPrototype = Uint8Array.prototype
    const typedArrayPrototype = objectGetPrototypeOf(uint8ArrayPrototype)
    const typedArrayLengthGetter = objectGetOwnPropertyDescriptor(
        typedArrayPrototype,
        "length"
    ).get
    const numberIsFinite = Number.isFinite
    const numberIsSafeInteger = Number.isSafeInteger
    const numberToString = Number.prototype.toString
    const stringConstructor = String
    const stringCharCodeAt = String.prototype.charCodeAt
    const jsonStringify = JSON.stringify
    const weakSetConstructor = WeakSet
    const weakSetAdd = WeakSet.prototype.add
    const weakSetDelete = WeakSet.prototype.delete
    const weakSetHas = WeakSet.prototype.has

    function apply(fn, receiver, args) {
        return reflectApply(fn, receiver, args)
    }

    function hasOwn(object, key) {
        return apply(objectHasOwnProperty, object, [key])
    }

    function append(array, value) {
        apply(arrayPush, array, [value])
    }

    function reject(kind, message) {
        throw { mqttPlusIssue: kind, message: message }
    }

    function validUnicode(value) {
        for (let index = 0; index < value.length; ++index) {
            const code = apply(stringCharCodeAt, value, [index])
            if (code >= 0xd800 && code <= 0xdbff) {
                if (index + 1 >= value.length) return false
                const next = apply(stringCharCodeAt, value, [index + 1])
                if (next < 0xdc00 || next > 0xdfff) return false
                ++index
            } else if (code >= 0xdc00 && code <= 0xdfff) {
                return false
            }
        }
        return true
    }

    function dataProperty(object, key, message) {
        const descriptor = objectGetOwnPropertyDescriptor(object, key)
        if (!descriptor || !hasOwn(descriptor, "value")) {
            reject("unsupported", message)
        }
        return descriptor.value
    }

    function byteHex(value) {
        let bytes = value
        const prototype = objectGetPrototypeOf(value)
        if (prototype === arrayBufferPrototype) {
            bytes = reflectConstruct(uint8ArrayConstructor, [value])
        } else if (prototype !== uint8ArrayPrototype) {
            reject("unsupported", "JavaScript result contains an unsupported binary view.")
        }

        const length = apply(typedArrayLengthGetter, bytes, [])
        let result = ""
        for (let index = 0; index < length; ++index) {
            const part = apply(numberToString, bytes[index], [16])
            if (part.length === 1) result += "0"
            result += part
        }
        return result
    }

    function encode(value, depth, state) {
        if (depth > state.maxDepth) {
            reject("limit", "JavaScript result nesting exceeds the configured limit.")
        }
        if (value === null) return ["null"]

        const type = typeof value
        if (type === "boolean") return ["boolean", value]
        if (type === "number") {
            if (!numberIsFinite(value)) {
                reject("unsupported", "JavaScript result contains a non-finite number.")
            }
            if (numberIsSafeInteger(value) && !objectIs(value, -0)) {
                return ["integer", value]
            }
            return ["double", value]
        }
        if (type === "string") {
            if (!validUnicode(value)) {
                reject("unsupported", "JavaScript result strings must contain valid Unicode.")
            }
            return ["string", value]
        }
        if (type !== "object") {
            reject("unsupported", "JavaScript returned an unsupported value type.")
        }

        const prototype = objectGetPrototypeOf(value)
        if (prototype === uint8ArrayPrototype || prototype === arrayBufferPrototype) {
            return ["bytes", byteHex(value)]
        }
        if (apply(weakSetHas, state.ancestors, [value])) {
            reject("unsupported", "JavaScript result must not contain cycles.")
        }
        apply(weakSetAdd, state.ancestors, [value])

        if (prototype === arrayPrototype) {
            const length = dataProperty(
                value,
                "length",
                "JavaScript result arrays must have a data length property."
            )
            if (!numberIsSafeInteger(length) || length < 0) {
                reject("unsupported", "JavaScript result has an invalid array length.")
            }
            if (length > state.maxEntries - state.entryCount) {
                reject("limit", "JavaScript result contains too many collection entries.")
            }
            state.entryCount += length

            const keys = reflectOwnKeys(value)
            if (keys.length !== length + 1) {
                reject("unsupported", "JavaScript result arrays must be dense and contain no extra properties.")
            }
            const items = []
            for (let index = 0; index < length; ++index) {
                append(items, encode(dataProperty(
                    value,
                    apply(stringConstructor, undefined, [index]),
                    "JavaScript result arrays must contain data elements only."
                ), depth + 1, state))
            }
            apply(weakSetDelete, state.ancestors, [value])
            return ["array", items]
        }

        if (prototype !== objectPrototype && prototype !== null) {
            reject("unsupported", "JavaScript result objects must be plain objects.")
        }
        const keys = reflectOwnKeys(value)
        if (keys.length > state.maxEntries - state.entryCount) {
            reject("limit", "JavaScript result contains too many collection entries.")
        }
        state.entryCount += keys.length
        const entries = []
        for (let index = 0; index < keys.length; ++index) {
            const key = keys[index]
            if (typeof key !== "string" || !validUnicode(key)) {
                reject("unsupported", "JavaScript result map keys must be Unicode strings.")
            }
            const descriptor = objectGetOwnPropertyDescriptor(value, key)
            if (!descriptor || !hasOwn(descriptor, "value") || !descriptor.enumerable) {
                reject("unsupported", "JavaScript result maps must contain enumerable data properties only.")
            }
            append(entries, [key, encode(descriptor.value, depth + 1, state)])
        }
        apply(weakSetDelete, state.ancestors, [value])
        return ["map", entries]
    }

    function issueText(error) {
        if (error !== null && typeof error === "object") {
            const issue = objectGetOwnPropertyDescriptor(error, "mqttPlusIssue")
            const message = objectGetOwnPropertyDescriptor(error, "message")
            if (issue && hasOwn(issue, "value") && message && hasOwn(message, "value")) {
                return (issue.value === "limit" ? "L" : "U") + message.value
            }
        }
        try {
            return "UJavaScript result inspection failed: "
                + apply(stringConstructor, undefined, [error])
        } catch (_) {
            return "UJavaScript result inspection failed."
        }
    }

    return {
        makeBytes: function(values) {
            return reflectConstruct(uint8ArrayConstructor, [values])
        },
        invoke: function(entry, context) {
            try {
                return [true, apply(entry, undefined, [context])]
            } catch (error) {
                return [false, error]
            }
        },
        normalize: function(value, maxDepth, maxEntries, maxEnvelopeCharacters) {
            try {
                const state = {
                    ancestors: reflectConstruct(weakSetConstructor, []),
                    entryCount: 0,
                    maxDepth: maxDepth,
                    maxEntries: maxEntries,
                }
                const encoded = encode(value, 0, state)
                const json = apply(jsonStringify, JSON, [encoded])
                if (json.length > maxEnvelopeCharacters) {
                    return "LJavaScript result exceeds the configured byte limit."
                }
                return "S" + json
            } catch (error) {
                return issueText(error)
            }
        },
    }
})()
)JS");
}

bool installHelpers(
    QJSEngine &engine,
    JavaScriptHelpers &helpers,
    JavaScriptFailure &failure)
{
    const QJSValue bootstrap = engine.evaluate(
        helperBootstrapSource(),
        QStringLiteral("mqtt-plus:javascript-runtime-bootstrap"));
    if (engine.hasError()) {
        const QJSValue error = engine.catchError();
        failure.message = error.property(QStringLiteral("message")).toString();
        if (failure.message.isEmpty()) {
            failure.message = error.toString();
        }
        return false;
    }
    helpers.makeBytes = bootstrap.property(QStringLiteral("makeBytes"));
    helpers.invoke = bootstrap.property(QStringLiteral("invoke"));
    helpers.normalize = bootstrap.property(QStringLiteral("normalize"));
    if (!helpers.makeBytes.isCallable()
        || !helpers.invoke.isCallable()
        || !helpers.normalize.isCallable()) {
        failure.message = QStringLiteral("Unable to initialize JavaScript runtime helpers.");
        return false;
    }
    return true;
}

void hideHostGlobals(QJSEngine &engine)
{
    QJSValue global = engine.globalObject();
    const QStringList names {
        QStringLiteral("Qt"),
        QStringLiteral("app"),
        QStringLiteral("console"),
        QStringLiteral("gc"),
        QStringLiteral("print"),
        QStringLiteral("require"),
        QStringLiteral("module"),
        QStringLiteral("XMLHttpRequest"),
    };
    for (const QString &name : names) {
        global.deleteProperty(name);
    }
}

JavaScriptFailure failureFromValue(
    const QJSValue &error,
    const QString &fallbackFile,
    const QStringList &stackTrace = {})
{
    JavaScriptFailure result;
    const QJSValue messageValue = error.property(QStringLiteral("message"));
    result.message = messageValue.isString()
        ? messageValue.toString()
        : error.toString();
    const QJSValue fileValue = error.property(QStringLiteral("fileName"));
    result.file = fileValue.isString() ? fileValue.toString() : QString();
    if (result.file.isEmpty()) {
        result.file = fallbackFile;
    }
    const QJSValue lineValue = error.property(QStringLiteral("lineNumber"));
    const QJSValue columnValue = error.property(QStringLiteral("columnNumber"));
    result.line = lineValue.isNumber() ? lineValue.toInt() : -1;
    result.column = columnValue.isNumber() ? columnValue.toInt() : -1;
    const QJSValue stackValue = error.property(QStringLiteral("stack"));
    result.stack = stackValue.isString() ? stackValue.toString() : QString();
    if (result.stack.isEmpty() && !stackTrace.isEmpty()) {
        result.stack = stackTrace.join(QLatin1Char('\n'));
    }
    if (result.line <= 0) {
        const QRegularExpression locationExpression(QStringLiteral(
            "(?:^|\\n)([^\\n:]+):(\\d+)(?::(\\d+))?"));
        const QRegularExpressionMatch match = locationExpression.match(
            result.stack + QStringLiteral("\n") + result.message);
        if (match.hasMatch()) {
            if (result.file.isEmpty()) {
                result.file = match.captured(1);
            }
            result.line = match.captured(2).toInt();
            if (result.column <= 0 && !match.captured(3).isEmpty()) {
                result.column = match.captured(3).toInt();
            }
        }
    }
    if (!result.stack.isEmpty() && !result.message.contains(result.stack)) {
        result.message += QStringLiteral("\n") + result.stack;
    }
    return result;
}

bool isJavaScriptSourceFile(const ProcessorSourceFile &file)
{
    return file.path.endsWith(QStringLiteral(".js"), Qt::CaseInsensitive)
        || file.path.endsWith(QStringLiteral(".mjs"), Qt::CaseInsensitive)
        || file.mediaType.contains(QStringLiteral("javascript"), Qt::CaseInsensitive)
        || file.mediaType.contains(QStringLiteral("ecmascript"), Qt::CaseInsensitive);
}

QVector<const ProcessorSourceFile *> orderedJavaScriptFiles(
    const ProcessorRevisionSnapshot &revision)
{
    QVector<const ProcessorSourceFile *> helperFiles;
    const ProcessorSourceFile *entryFile = nullptr;
    for (const ProcessorSourceFile &file : revision.files) {
        if (file.path == revision.entryFile) {
            entryFile = &file;
        } else if (isJavaScriptSourceFile(file)) {
            helperFiles.append(&file);
        }
    }
    std::sort(
        helperFiles.begin(),
        helperFiles.end(),
        [](const ProcessorSourceFile *left, const ProcessorSourceFile *right) {
            return left->path < right->path;
        });
    if (entryFile && isJavaScriptSourceFile(*entryFile)) {
        helperFiles.append(entryFile);
    }
    return helperFiles;
}

bool decodeSourceFile(
    const ProcessorSourceFile &file,
    JavaScriptSource &source,
    JavaScriptFailure &failure)
{
    if (file.path.contains(QChar::Null)) {
        failure.message = QStringLiteral("JavaScript source path contains an invalid character.");
        failure.file = file.path;
        return false;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    source.source = decoder.decode(file.content);
    if (decoder.hasError()) {
        failure.message = QStringLiteral("JavaScript source must be valid UTF-8.");
        failure.file = file.path;
        return false;
    }
    source.path = file.path;
    return true;
}

bool evaluateSources(
    QJSEngine &engine,
    const QVector<JavaScriptSource> &sources,
    JavaScriptFailure &failure,
    bool &syntaxError)
{
    for (const JavaScriptSource &source : sources) {
        QStringList stackTrace;
        const QJSValue evaluationResult = engine.evaluate(
            source.source,
            source.path,
            1,
            &stackTrace);
        const bool returnedException = !stackTrace.isEmpty()
            || (evaluationResult.isError()
                && evaluationResult.errorType() == QJSValue::SyntaxError);
        if (!engine.hasError() && !returnedException) {
            continue;
        }
        const QJSValue error = engine.hasError()
            ? engine.catchError()
            : evaluationResult;
        syntaxError = error.errorType() == QJSValue::SyntaxError;
        failure = failureFromValue(error, source.path, stackTrace);
        return false;
    }
    return true;
}

bool resolveEntry(
    QJSEngine &engine,
    const QString &entrySymbol,
    const QString &entryFile,
    QJSValue &entry,
    JavaScriptFailure &failure)
{
    entry = engine.globalObject().property(entrySymbol);
    if (engine.hasError()) {
        failure = failureFromValue(engine.catchError(), entryFile);
        return false;
    }
    if (!entry.isCallable()) {
        failure.message = QStringLiteral(
            "JavaScript processor must define function %1(context).")
                              .arg(entrySymbol);
        failure.file = entryFile;
        return false;
    }
    return true;
}

bool cborToJavaScript(
    QJSEngine &engine,
    const JavaScriptHelpers &helpers,
    const QCborValue &value,
    int depth,
    qsizetype &entryCount,
    const ProcessorExecutionLimits &limits,
    QJSValue &result,
    QString &error)
{
    if (depth > limits.maxResultDepth) {
        error = QStringLiteral("Processor parameters nesting exceeds the configured limit.");
        return false;
    }

    switch (value.type()) {
    case QCborValue::Null:
        result = QJSValue(QJSValue::NullValue);
        return true;
    case QCborValue::False:
    case QCborValue::True:
        result = QJSValue(value.toBool());
        return true;
    case QCborValue::Integer: {
        const qint64 integer = value.toInteger();
        if (integer < -kMaximumSafeJavaScriptInteger
            || integer > kMaximumSafeJavaScriptInteger) {
            error = QStringLiteral(
                "Processor parameters contain an integer outside JavaScript's safe range.");
            return false;
        }
        result = QJSValue(static_cast<double>(integer));
        return true;
    }
    case QCborValue::Double:
        if (!std::isfinite(value.toDouble())) {
            error = QStringLiteral("Processor parameters contain a non-finite number.");
            return false;
        }
        result = QJSValue(value.toDouble());
        return true;
    case QCborValue::String:
        result = QJSValue(value.toString());
        return true;
    case QCborValue::ByteArray: {
        const QByteArray bytes = value.toByteArray();
        QJSValue values = engine.newArray(static_cast<uint>(bytes.size()));
        for (qsizetype index = 0; index < bytes.size(); ++index) {
            values.setProperty(
                static_cast<quint32>(index),
                QJSValue(static_cast<int>(static_cast<unsigned char>(bytes.at(index)))));
        }
        result = helpers.makeBytes.call({values});
        if (engine.hasError()) {
            engine.catchError();
            error = QStringLiteral("Unable to create a JavaScript byte array.");
            return false;
        }
        return true;
    }
    case QCborValue::Array: {
        const QCborArray array = value.toArray();
        if (array.size() > limits.maxCollectionEntries - entryCount) {
            error = QStringLiteral("Processor parameters contain too many collection entries.");
            return false;
        }
        entryCount += array.size();
        result = engine.newArray(static_cast<uint>(array.size()));
        for (qsizetype index = 0; index < array.size(); ++index) {
            QJSValue item;
            if (!cborToJavaScript(
                    engine,
                    helpers,
                    array.at(index),
                    depth + 1,
                    entryCount,
                    limits,
                    item,
                    error)) {
                return false;
            }
            result.setProperty(static_cast<quint32>(index), item);
        }
        return true;
    }
    case QCborValue::Map: {
        const QCborMap map = value.toMap();
        if (map.size() > limits.maxCollectionEntries - entryCount) {
            error = QStringLiteral("Processor parameters contain too many collection entries.");
            return false;
        }
        entryCount += map.size();
        result = engine.newObject();
        result.setPrototype(QJSValue(QJSValue::NullValue));
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            if (!it.key().isString()) {
                error = QStringLiteral("Processor parameter map keys must be strings.");
                return false;
            }
            QJSValue item;
            if (!cborToJavaScript(
                    engine,
                    helpers,
                    it.value(),
                    depth + 1,
                    entryCount,
                    limits,
                    item,
                    error)) {
                return false;
            }
            result.setProperty(it.key().toString(), item);
        }
        return true;
    }
    default:
        error = QStringLiteral("Processor parameters contain an unsupported value type.");
        return false;
    }
}

bool createContext(
    QJSEngine &engine,
    const JavaScriptHelpers &helpers,
    const MessageProcessorContext &context,
    const ProcessorExecutionLimits &limits,
    QJSValue &result,
    QString &error)
{
    result = engine.newObject();
    result.setPrototype(QJSValue(QJSValue::NullValue));
    result.setProperty(QStringLiteral("topic"), context.topic);

    QJSValue payload;
    qsizetype entryCount = 0;
    if (!cborToJavaScript(
            engine,
            helpers,
            QCborValue(context.payload),
            0,
            entryCount,
            limits,
            payload,
            error)) {
        return false;
    }
    result.setProperty(QStringLiteral("payload"), payload);
    result.setProperty(QStringLiteral("receivedAt"), context.receivedAt);
    result.setProperty(QStringLiteral("format"), context.format);
    result.setProperty(QStringLiteral("decoded"), context.decoded);
    result.setProperty(QStringLiteral("decodeError"), context.decodeError);

    QJSValue parameters;
    entryCount = 0;
    if (!cborToJavaScript(
            engine,
            helpers,
            QCborValue(context.parameters),
            0,
            entryCount,
            limits,
            parameters,
            error)) {
        return false;
    }
    result.setProperty(QStringLiteral("parameters"), parameters);
    return true;
}

bool jsonArrayWithTag(
    const QJsonValue &value,
    QString &tag,
    QJsonArray &array)
{
    if (!value.isArray()) {
        return false;
    }
    array = value.toArray();
    if (array.isEmpty() || !array.at(0).isString()) {
        return false;
    }
    tag = array.at(0).toString();
    return true;
}

JavaScriptValueResult envelopeToCbor(
    const QJsonValue &envelope,
    int depth,
    qsizetype &entryCount,
    const ProcessorExecutionLimits &limits)
{
    if (depth > limits.maxResultDepth) {
        return {
            JavaScriptValueIssue::LimitExceeded,
            {},
            QStringLiteral("JavaScript result nesting exceeds the configured limit."),
        };
    }

    QString tag;
    QJsonArray value;
    if (!jsonArrayWithTag(envelope, tag, value)) {
        return {
            JavaScriptValueIssue::InvalidEnvelope,
            {},
            QStringLiteral("JavaScript runtime produced an invalid result envelope."),
        };
    }
    if (tag == QStringLiteral("null") && value.size() == 1) {
        return {JavaScriptValueIssue::None, QCborValue(nullptr), {}};
    }
    if (tag == QStringLiteral("boolean")
        && value.size() == 2
        && value.at(1).isBool()) {
        return {JavaScriptValueIssue::None, QCborValue(value.at(1).toBool()), {}};
    }
    if ((tag == QStringLiteral("integer") || tag == QStringLiteral("double"))
        && value.size() == 2
        && value.at(1).isDouble()) {
        const double number = value.at(1).toDouble();
        if (!std::isfinite(number)) {
            return {
                JavaScriptValueIssue::InvalidEnvelope,
                {},
                QStringLiteral("JavaScript runtime produced an invalid number."),
            };
        }
        if (tag == QStringLiteral("integer")) {
            if (std::trunc(number) != number
                || std::abs(number) > kMaximumSafeJavaScriptInteger) {
                return {
                    JavaScriptValueIssue::InvalidEnvelope,
                    {},
                    QStringLiteral("JavaScript runtime produced an invalid integer."),
                };
            }
            return {
                JavaScriptValueIssue::None,
                QCborValue(static_cast<qint64>(number)),
                {},
            };
        }
        return {JavaScriptValueIssue::None, QCborValue(number), {}};
    }
    if (tag == QStringLiteral("string")
        && value.size() == 2
        && value.at(1).isString()) {
        return {
            JavaScriptValueIssue::None,
            QCborValue(value.at(1).toString()),
            {},
        };
    }
    if (tag == QStringLiteral("bytes")
        && value.size() == 2
        && value.at(1).isString()) {
        const QByteArray hex = value.at(1).toString().toLatin1();
        const auto isHex = [](char character) {
            return (character >= '0' && character <= '9')
                || (character >= 'a' && character <= 'f');
        };
        if (hex.size() % 2 != 0
            || !std::all_of(hex.cbegin(), hex.cend(), isHex)) {
            return {
                JavaScriptValueIssue::InvalidEnvelope,
                {},
                QStringLiteral("JavaScript runtime produced invalid byte data."),
            };
        }
        return {
            JavaScriptValueIssue::None,
            QCborValue(QByteArray::fromHex(hex)),
            {},
        };
    }
    if (tag == QStringLiteral("array")
        && value.size() == 2
        && value.at(1).isArray()) {
        const QJsonArray items = value.at(1).toArray();
        if (items.size() > limits.maxCollectionEntries - entryCount) {
            return {
                JavaScriptValueIssue::LimitExceeded,
                {},
                QStringLiteral("JavaScript result contains too many collection entries."),
            };
        }
        entryCount += items.size();
        QCborArray result;
        for (const QJsonValue &itemValue : items) {
            const JavaScriptValueResult item = envelopeToCbor(
                itemValue,
                depth + 1,
                entryCount,
                limits);
            if (item.issue != JavaScriptValueIssue::None) {
                return item;
            }
            result.append(item.value);
        }
        return {JavaScriptValueIssue::None, result, {}};
    }
    if (tag == QStringLiteral("map")
        && value.size() == 2
        && value.at(1).isArray()) {
        const QJsonArray entries = value.at(1).toArray();
        if (entries.size() > limits.maxCollectionEntries - entryCount) {
            return {
                JavaScriptValueIssue::LimitExceeded,
                {},
                QStringLiteral("JavaScript result contains too many collection entries."),
            };
        }
        entryCount += entries.size();
        QCborMap result;
        QSet<QString> keys;
        for (const QJsonValue &entryValue : entries) {
            if (!entryValue.isArray()) {
                return {
                    JavaScriptValueIssue::InvalidEnvelope,
                    {},
                    QStringLiteral("JavaScript runtime produced an invalid map entry."),
                };
            }
            const QJsonArray entry = entryValue.toArray();
            if (entry.size() != 2 || !entry.at(0).isString()) {
                return {
                    JavaScriptValueIssue::InvalidEnvelope,
                    {},
                    QStringLiteral("JavaScript runtime produced an invalid map entry."),
                };
            }
            const QString key = entry.at(0).toString();
            if (keys.contains(key)) {
                return {
                    JavaScriptValueIssue::InvalidEnvelope,
                    {},
                    QStringLiteral("JavaScript runtime produced duplicate map keys."),
                };
            }
            keys.insert(key);
            const JavaScriptValueResult item = envelopeToCbor(
                entry.at(1),
                depth + 1,
                entryCount,
                limits);
            if (item.issue != JavaScriptValueIssue::None) {
                return item;
            }
            result.insert(key, item.value);
        }
        return {JavaScriptValueIssue::None, result, {}};
    }

    return {
        JavaScriptValueIssue::InvalidEnvelope,
        {},
        QStringLiteral("JavaScript runtime produced an invalid result envelope."),
    };
}

JavaScriptValueResult normalizedValue(
    const QString &normalized,
    const ProcessorExecutionLimits &limits)
{
    if (normalized.isEmpty()) {
        return {
            JavaScriptValueIssue::InvalidEnvelope,
            {},
            QStringLiteral("JavaScript runtime produced no normalized result."),
        };
    }
    const QChar state = normalized.at(0);
    const QString body = normalized.sliced(1);
    if (state == QLatin1Char('U')) {
        return {JavaScriptValueIssue::Unsupported, {}, body};
    }
    if (state == QLatin1Char('L')) {
        return {JavaScriptValueIssue::LimitExceeded, {}, body};
    }
    if (state != QLatin1Char('S')) {
        return {
            JavaScriptValueIssue::InvalidEnvelope,
            {},
            QStringLiteral("JavaScript runtime produced an invalid result state."),
        };
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        return {
            JavaScriptValueIssue::InvalidEnvelope,
            {},
            QStringLiteral("JavaScript runtime produced invalid normalized data."),
        };
    }
    qsizetype entryCount = 0;
    return envelopeToCbor(document.array(), 0, entryCount, limits);
}

ProcessorPreparationResult preparationFailure(
    ProcessorValidationState state,
    const QString &code,
    const JavaScriptFailure &failure)
{
    ProcessorPreparationResult result;
    result.state = state;
    result.diagnostics.append(diagnostic(
        code,
        failure.message,
        failure.file,
        failure.line,
        failure.column));
    return result;
}

ProcessorExecutionResult executionFailure(
    ProcessorExecutionState state,
    const QString &code,
    const JavaScriptFailure &failure)
{
    ProcessorExecutionResult result;
    result.state = state;
    result.diagnostics.append(diagnostic(
        code,
        failure.message,
        failure.file,
        failure.line,
        failure.column));
    return result;
}

} // namespace

RuntimeDescriptor JavaScriptRuntimeAdapter::descriptor() const
{
    RuntimeDescriptor result;
    result.runtimeId = QStringLiteral("qt-qjs");
    result.languageId = QStringLiteral("javascript");
    result.displayName = QStringLiteral("JavaScript (Qt QJSEngine)");
    result.runtimeVersion = QStringLiteral("Qt %1 QJSEngine")
                                .arg(QString::fromLatin1(qVersion()));
    result.supportedContractIds = {
        QStringLiteral("mqtt-plus.message-processor/v1"),
    };
    result.sourceExtensions = {
        QStringLiteral("js"),
        QStringLiteral("mjs"),
    };
    result.executionMode = RuntimeExecutionMode::ParseWorkerThread;
    return result;
}

ProcessorPreparationResult JavaScriptRuntimeAdapter::prepare(
    const ProcessorRevisionSnapshot &revision,
    const ProcessorExecutionLimits &limits)
{
    const QVector<const ProcessorSourceFile *> sourceFiles = orderedJavaScriptFiles(revision);
    if (sourceFiles.isEmpty()
        || sourceFiles.last()->path != revision.entryFile
        || revision.entrySymbol.contains(QChar::Null)) {
        ProcessorPreparationResult result;
        result.state = ProcessorValidationState::InvalidSource;
        result.diagnostics.append(diagnostic(
            QStringLiteral("invalid_source"),
            QStringLiteral("JavaScript processor entry file or entry symbol is invalid."),
            revision.entryFile));
        return result;
    }

    auto prepared = QSharedPointer<JavaScriptPreparedProcessor>::create();
    prepared->entrySymbol = revision.entrySymbol;
    prepared->entryFile = revision.entryFile;
    prepared->sources.reserve(sourceFiles.size());
    for (const ProcessorSourceFile *sourceFile : sourceFiles) {
        JavaScriptSource source;
        JavaScriptFailure failure;
        if (!decodeSourceFile(*sourceFile, source, failure)) {
            return preparationFailure(
                ProcessorValidationState::InvalidSource,
                QStringLiteral("invalid_source"),
                failure);
        }
        prepared->sources.append(std::move(source));
    }

    QJSEngine engine;
    hideHostGlobals(engine);
    JavaScriptHelpers helpers;
    JavaScriptFailure failure;
    if (!installHelpers(engine, helpers, failure)) {
        return preparationFailure(
            ProcessorValidationState::InternalError,
            QStringLiteral("internal_runtime_error"),
            failure);
    }

    JavaScriptWatchdog watchdog(engine, limits.wallTimeMilliseconds);
    bool syntaxError = false;
    if (!evaluateSources(engine, prepared->sources, failure, syntaxError)) {
        watchdog.stop();
        if (watchdog.timedOut()) {
            failure.message = QStringLiteral("JavaScript preparation timed out.");
            return preparationFailure(
                ProcessorValidationState::PreparationFailed,
                QStringLiteral("preparation_failed"),
                failure);
        }
        return preparationFailure(
            syntaxError
                ? ProcessorValidationState::InvalidSource
                : ProcessorValidationState::PreparationFailed,
            syntaxError
                ? QStringLiteral("invalid_source")
                : QStringLiteral("preparation_failed"),
            failure);
    }

    QJSValue entry;
    if (!resolveEntry(
            engine,
            prepared->entrySymbol,
            prepared->entryFile,
            entry,
            failure)) {
        watchdog.stop();
        if (watchdog.timedOut()) {
            failure.message = QStringLiteral("JavaScript preparation timed out.");
            return preparationFailure(
                ProcessorValidationState::PreparationFailed,
                QStringLiteral("preparation_failed"),
                failure);
        }
        return preparationFailure(
            ProcessorValidationState::InvalidSource,
            QStringLiteral("invalid_source"),
            failure);
    }
    watchdog.stop();
    if (watchdog.timedOut()) {
        failure.message = QStringLiteral("JavaScript preparation timed out.");
        return preparationFailure(
            ProcessorValidationState::PreparationFailed,
            QStringLiteral("preparation_failed"),
            failure);
    }

    ProcessorPreparationResult result;
    result.state = ProcessorValidationState::Ready;
    result.prepared = prepared;
    return result;
}

ProcessorExecutionResult JavaScriptRuntimeAdapter::execute(
    const PreparedProcessorHandle &prepared,
    const MessageProcessorContext &context,
    const ProcessorExecutionLimits &limits)
{
    const auto *javascriptPrepared = dynamic_cast<const JavaScriptPreparedProcessor *>(
        prepared.data());
    if (!javascriptPrepared) {
        ProcessorExecutionResult result;
        result.state = ProcessorExecutionState::InternalError;
        result.diagnostics.append(diagnostic(
            QStringLiteral("internal_runtime_error"),
            QStringLiteral(
                "JavaScript runtime received an incompatible prepared processor.")));
        return result;
    }

    QJSEngine engine;
    hideHostGlobals(engine);
    JavaScriptHelpers helpers;
    JavaScriptFailure failure;
    if (!installHelpers(engine, helpers, failure)) {
        return executionFailure(
            ProcessorExecutionState::InternalError,
            QStringLiteral("internal_runtime_error"),
            failure);
    }

    QJSValue javascriptContext;
    QString contextError;
    if (!createContext(
            engine,
            helpers,
            context,
            limits,
            javascriptContext,
            contextError)) {
        failure.message = contextError;
        return executionFailure(
            ProcessorExecutionState::ExecutionFailed,
            QStringLiteral("execution_failed"),
            failure);
    }

    JavaScriptWatchdog watchdog(engine, limits.wallTimeMilliseconds);
    bool syntaxError = false;
    if (!evaluateSources(engine, javascriptPrepared->sources, failure, syntaxError)) {
        watchdog.stop();
        if (watchdog.timedOut()) {
            failure.message = QStringLiteral("JavaScript execution limit exceeded.");
            return executionFailure(
                ProcessorExecutionState::TimedOut,
                QStringLiteral("execution_timed_out"),
                failure);
        }
        return executionFailure(
            ProcessorExecutionState::ExecutionFailed,
            QStringLiteral("execution_failed"),
            failure);
    }

    QJSValue entry;
    if (!resolveEntry(
            engine,
            javascriptPrepared->entrySymbol,
            javascriptPrepared->entryFile,
            entry,
            failure)) {
        watchdog.stop();
        if (watchdog.timedOut()) {
            failure.message = QStringLiteral("JavaScript execution limit exceeded.");
            return executionFailure(
                ProcessorExecutionState::TimedOut,
                QStringLiteral("execution_timed_out"),
                failure);
        }
        return executionFailure(
            ProcessorExecutionState::ExecutionFailed,
            QStringLiteral("execution_failed"),
            failure);
    }

    const QJSValue invocation = helpers.invoke.call({entry, javascriptContext});
    if (engine.hasError()) {
        const QJSValue error = engine.catchError();
        failure = failureFromValue(error, javascriptPrepared->entryFile);
        watchdog.stop();
        if (watchdog.timedOut()) {
            failure.message = QStringLiteral("JavaScript execution limit exceeded.");
            return executionFailure(
                ProcessorExecutionState::TimedOut,
                QStringLiteral("execution_timed_out"),
                failure);
        }
        return executionFailure(
            ProcessorExecutionState::ExecutionFailed,
            QStringLiteral("execution_failed"),
            failure);
    }
    if (!invocation.isArray()) {
        watchdog.stop();
        if (watchdog.timedOut()) {
            failure.message = QStringLiteral("JavaScript execution limit exceeded.");
            return executionFailure(
                ProcessorExecutionState::TimedOut,
                QStringLiteral("execution_timed_out"),
                failure);
        }
        failure.message = QStringLiteral(
            "JavaScript runtime produced an invalid invocation result.");
        return executionFailure(
            ProcessorExecutionState::InternalError,
            QStringLiteral("internal_runtime_error"),
            failure);
    }
    const bool invocationSucceeded = invocation.property(0).toBool();
    QJSValue rawResult = invocation.property(1);
    if (!invocationSucceeded) {
        failure = failureFromValue(rawResult, javascriptPrepared->entryFile);
        watchdog.stop();
        if (watchdog.timedOut()) {
            failure.message = QStringLiteral("JavaScript execution limit exceeded.");
            return executionFailure(
                ProcessorExecutionState::TimedOut,
                QStringLiteral("execution_timed_out"),
                failure);
        }
        return executionFailure(
            ProcessorExecutionState::ExecutionFailed,
            QStringLiteral("execution_failed"),
            failure);
    }

    const qsizetype maximumEnvelopeCharacters =
        limits.maxResultBytes > ((std::numeric_limits<qsizetype>::max)()
                - kEnvelopeSlackCharacters) / kEnvelopeExpansionFactor
        ? (std::numeric_limits<qsizetype>::max)()
        : limits.maxResultBytes * kEnvelopeExpansionFactor
            + kEnvelopeSlackCharacters;
    const QJSValue normalizedResult = helpers.normalize.call({
        rawResult,
        QJSValue(limits.maxResultDepth),
        QJSValue(static_cast<double>(limits.maxCollectionEntries)),
        QJSValue(static_cast<double>(maximumEnvelopeCharacters)),
    });
    if (engine.hasError()) {
        failure = failureFromValue(engine.catchError(), javascriptPrepared->entryFile);
        watchdog.stop();
        if (watchdog.timedOut()) {
            failure.message = QStringLiteral("JavaScript execution limit exceeded.");
            return executionFailure(
                ProcessorExecutionState::TimedOut,
                QStringLiteral("execution_timed_out"),
                failure);
        }
        return executionFailure(
            ProcessorExecutionState::ExecutionFailed,
            QStringLiteral("execution_failed"),
            failure);
    }
    watchdog.stop();
    if (watchdog.timedOut()) {
        failure.message = QStringLiteral("JavaScript execution limit exceeded.");
        return executionFailure(
            ProcessorExecutionState::TimedOut,
            QStringLiteral("execution_timed_out"),
            failure);
    }
    if (!normalizedResult.isString()) {
        failure.message = QStringLiteral(
            "JavaScript runtime produced an invalid normalized result.");
        return executionFailure(
            ProcessorExecutionState::InternalError,
            QStringLiteral("internal_runtime_error"),
            failure);
    }

    const JavaScriptValueResult value = normalizedValue(
        normalizedResult.toString(),
        limits);
    if (value.issue != JavaScriptValueIssue::None) {
        ProcessorExecutionResult result;
        result.state = value.issue == JavaScriptValueIssue::LimitExceeded
            ? ProcessorExecutionState::OutputLimitExceeded
            : value.issue == JavaScriptValueIssue::Unsupported
                ? ProcessorExecutionState::UnsupportedResult
                : ProcessorExecutionState::InternalError;
        result.diagnostics.append(diagnostic(
            value.issue == JavaScriptValueIssue::LimitExceeded
                ? QStringLiteral("result_limit_exceeded")
                : value.issue == JavaScriptValueIssue::Unsupported
                    ? QStringLiteral("unsupported_result")
                    : QStringLiteral("internal_runtime_error"),
            value.message));
        return result;
    }

    ProcessorExecutionResult result;
    result.state = ProcessorExecutionState::Succeeded;
    result.value = value.value;
    return result;
}
