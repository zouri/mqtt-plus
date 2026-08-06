#include "services/processors/messageprocessorengine.h"
#include "services/processors/runtimes/luaruntimeadapter.h"
#include "support/processorruntimeconformance.h"

#include <QtTest/QtTest>

#include <QCborMap>
#include <QCryptographicHash>

#include <algorithm>
#include <utility>

namespace {

ProcessorRevisionSnapshot luaRevision(
    const QByteArray &entrySource,
    QVector<ProcessorSourceFile> helperFiles = {},
    const QString &entrySymbol = QStringLiteral("process"))
{
    ProcessorSourceFile entryFile;
    entryFile.path = QStringLiteral("main.lua");
    entryFile.mediaType = QStringLiteral("text/x-lua");
    entryFile.content = entrySource;
    helperFiles.append(entryFile);

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(entrySymbol.toUtf8());
    for (const ProcessorSourceFile &file : std::as_const(helperFiles)) {
        hash.addData(file.path.toUtf8());
        hash.addData(file.content);
    }
    const QString contentHash = QString::fromLatin1(hash.result().toHex());

    ProcessorRevisionSnapshot revision;
    revision.id = QStringLiteral("lua-revision-%1").arg(contentHash.left(12));
    revision.processorId = QStringLiteral("lua-processor");
    revision.revisionNumber = 1;
    revision.contractId = QStringLiteral("mqtt-plus.message-processor/v1");
    revision.languageId = QStringLiteral("lua");
    revision.runtimeId = QStringLiteral("lua-5.5");
    revision.entryFile = entryFile.path;
    revision.entrySymbol = entrySymbol;
    revision.contentHash = contentHash;
    revision.files = std::move(helperFiles);
    revision.createdAt = QStringLiteral("2026-08-05T08:00:00.000Z");
    return revision;
}

QSharedPointer<LuaRuntimeAdapter> luaAdapter()
{
    return QSharedPointer<LuaRuntimeAdapter>::create();
}

const ProcessorDiagnostic *diagnosticByCode(
    const QVector<ProcessorDiagnostic> &diagnostics,
    const QString &code)
{
    const auto it = std::find_if(
        diagnostics.cbegin(),
        diagnostics.cend(),
        [&code](const ProcessorDiagnostic &diagnostic) {
            return diagnostic.code == code;
        });
    return it == diagnostics.cend() ? nullptr : &*it;
}

} // namespace

class LuaRuntimeAdapterTest : public QObject
{
    Q_OBJECT

private slots:
    void describesTheLuaRuntime();
    void passesCommonRuntimeConformanceHarness();
    void isolatesStateAndRecoversAfterRuntimeErrors();
    void timesOutAndRecoversForTheNextMessage();
    void loadsMultipleFilesBeforeTheEntryFile();
    void rejectsInvalidSourceDuringPreparation();
    void rejectsHostileAndOversizedResults();
    void ignoresResultMetatablesAndHidesUnsafeLibraries();
};

void LuaRuntimeAdapterTest::describesTheLuaRuntime()
{
    const RuntimeDescriptor descriptor = luaAdapter()->descriptor();
    QCOMPARE(descriptor.runtimeId, QStringLiteral("lua-5.5"));
    QCOMPARE(descriptor.languageId, QStringLiteral("lua"));
    QVERIFY(descriptor.runtimeVersion.startsWith(QStringLiteral("Lua 5.5")));
    QVERIFY(descriptor.supportedContractIds.contains(
        QStringLiteral("mqtt-plus.message-processor/v1")));
    QCOMPARE(
        static_cast<int>(descriptor.executionMode),
        static_cast<int>(RuntimeExecutionMode::ParseWorkerThread));
}

void LuaRuntimeAdapterTest::passesCommonRuntimeConformanceHarness()
{
    const ProcessorRevisionSnapshot revision = luaRevision(QByteArrayLiteral(
        "function process(context)\n"
        "    return {\n"
        "        context = {\n"
        "            topic = context.topic,\n"
        "            payload = bytes(context.payload),\n"
        "            receivedAt = context.receivedAt,\n"
        "            format = context.format,\n"
        "            decoded = context.decoded,\n"
        "            decodeError = context.decodeError,\n"
        "            parameters = context.parameters,\n"
        "        },\n"
        "        values = { null, true, 42, 3.5, 'value', bytes('abc') },\n"
        "    }\n"
        "end\n"));

    ProcessorRuntimeConformance::verify(luaAdapter(), revision);
}

void LuaRuntimeAdapterTest::isolatesStateAndRecoversAfterRuntimeErrors()
{
    const ProcessorRevisionSnapshot revision = luaRevision(QByteArrayLiteral(
        "local calls = 0\n"
        "function process(context)\n"
        "    calls = calls + 1\n"
        "    if context.topic == 'bad' then error('bad payload') end\n"
        "    return calls\n"
        "end\n"));
    MessageProcessorEngine engine({luaAdapter()});

    MessageProcessorContext context = ProcessorRuntimeConformance::context();
    const ProcessorExecutionResult first = engine.execute(revision, context);
    QVERIFY2(first.succeeded(), qPrintable(first.diagnostics.value(0).message));
    QCOMPARE(first.value.toInteger(), qint64(1));

    context.topic = QStringLiteral("bad");
    const ProcessorExecutionResult failed = engine.execute(revision, context);
    QCOMPARE(
        static_cast<int>(failed.state),
        static_cast<int>(ProcessorExecutionState::ExecutionFailed));
    const ProcessorDiagnostic *failure = diagnosticByCode(
        failed.diagnostics,
        QStringLiteral("execution_failed"));
    QVERIFY(failure);
    QVERIFY(failure->message.contains(QStringLiteral("bad payload")));
    QVERIFY(failure->file.endsWith(QStringLiteral("main.lua")));
    QVERIFY(failure->line > 0);

    context.topic = QStringLiteral("good");
    const ProcessorExecutionResult recovered = engine.execute(revision, context);
    QVERIFY2(recovered.succeeded(), qPrintable(recovered.diagnostics.value(0).message));
    QCOMPARE(recovered.value.toInteger(), qint64(1));
}

void LuaRuntimeAdapterTest::timesOutAndRecoversForTheNextMessage()
{
    const ProcessorRevisionSnapshot revision = luaRevision(QByteArrayLiteral(
        "function process(context)\n"
        "    if context.topic == 'timeout' then while true do end end\n"
        "    return 'ok'\n"
        "end\n"));
    MessageProcessorEngine engine({luaAdapter()});
    ProcessorExecutionLimits limits;
    limits.wallTimeMilliseconds = 10;

    MessageProcessorContext context = ProcessorRuntimeConformance::context();
    context.topic = QStringLiteral("timeout");
    const ProcessorExecutionResult timedOut = engine.execute(revision, context, limits);
    QCOMPARE(
        static_cast<int>(timedOut.state),
        static_cast<int>(ProcessorExecutionState::TimedOut));
    QVERIFY(diagnosticByCode(
        timedOut.diagnostics,
        QStringLiteral("execution_timed_out")));

    context.topic = QStringLiteral("normal");
    const ProcessorExecutionResult recovered = engine.execute(revision, context, limits);
    QVERIFY2(recovered.succeeded(), qPrintable(recovered.diagnostics.value(0).message));
    QCOMPARE(recovered.value.toString(), QStringLiteral("ok"));
}

void LuaRuntimeAdapterTest::loadsMultipleFilesBeforeTheEntryFile()
{
    ProcessorSourceFile helper;
    helper.path = QStringLiteral("lib/protocol.lua");
    helper.mediaType = QStringLiteral("text/x-lua");
    helper.content = QByteArrayLiteral(
        "function apply_scale(value, scale) return value * scale end\n");
    const ProcessorRevisionSnapshot revision = luaRevision(
        QByteArrayLiteral(
            "function transform(context)\n"
            "    return apply_scale(4, context.parameters.scale)\n"
            "end\n"),
        {helper},
        QStringLiteral("transform"));
    MessageProcessorEngine engine({luaAdapter()});

    MessageProcessorContext context = ProcessorRuntimeConformance::context();
    const ProcessorExecutionResult result = engine.execute(revision, context);
    QVERIFY2(result.succeeded(), qPrintable(result.diagnostics.value(0).message));
    QCOMPARE(result.value.toInteger(), qint64(40));
}

void LuaRuntimeAdapterTest::rejectsInvalidSourceDuringPreparation()
{
    MessageProcessorEngine engine({luaAdapter()});

    const ProcessorRevisionSnapshot syntaxError = luaRevision(QByteArrayLiteral(
        "function process(context)\n"
        "    return {\n"
        "end\n"));
    const ProcessorValidationResult syntaxResult = engine.validate(syntaxError);
    QCOMPARE(
        static_cast<int>(syntaxResult.state),
        static_cast<int>(ProcessorValidationState::InvalidSource));
    const ProcessorDiagnostic *syntaxDiagnostic = diagnosticByCode(
        syntaxResult.diagnostics,
        QStringLiteral("invalid_source"));
    QVERIFY(syntaxDiagnostic);
    QVERIFY(syntaxDiagnostic->file.endsWith(QStringLiteral("main.lua")));
    QVERIFY(syntaxDiagnostic->line > 0);

    const ProcessorRevisionSnapshot missingEntry = luaRevision(QByteArrayLiteral(
        "function another_function(context) return 1 end\n"));
    const ProcessorValidationResult missingResult = engine.validate(missingEntry);
    QCOMPARE(
        static_cast<int>(missingResult.state),
        static_cast<int>(ProcessorValidationState::InvalidSource));
    QVERIFY(diagnosticByCode(
        missingResult.diagnostics,
        QStringLiteral("invalid_source")));

    const ProcessorRevisionSnapshot topLevelFailure = luaRevision(QByteArrayLiteral(
        "error('top-level failure')\n"
        "function process(context) return 1 end\n"));
    const ProcessorValidationResult preparationResult = engine.validate(topLevelFailure);
    QCOMPARE(
        static_cast<int>(preparationResult.state),
        static_cast<int>(ProcessorValidationState::PreparationFailed));
    QVERIFY(diagnosticByCode(
        preparationResult.diagnostics,
        QStringLiteral("preparation_failed")));
}

void LuaRuntimeAdapterTest::rejectsHostileAndOversizedResults()
{
    struct Case
    {
        QByteArray source;
        ProcessorExecutionState state;
        QString code;
        ProcessorExecutionLimits limits;
    };

    ProcessorExecutionLimits smallEntryLimit;
    smallEntryLimit.maxCollectionEntries = 2;
    ProcessorExecutionLimits shallowLimit;
    shallowLimit.maxResultDepth = 1;
    const QVector<Case> cases {
        {
            QByteArrayLiteral("function process(context) return function() end end\n"),
            ProcessorExecutionState::UnsupportedResult,
            QStringLiteral("unsupported_result"),
            {},
        },
        {
            QByteArrayLiteral(
                "function process(context) local value = {}; value.self = value; return value end\n"),
            ProcessorExecutionState::UnsupportedResult,
            QStringLiteral("unsupported_result"),
            {},
        },
        {
            QByteArrayLiteral("function process(context) return { [2] = 'value' } end\n"),
            ProcessorExecutionState::UnsupportedResult,
            QStringLiteral("unsupported_result"),
            {},
        },
        {
            QByteArrayLiteral(
                "function process(context) return { [string.char(255)] = 'value' } end\n"),
            ProcessorExecutionState::UnsupportedResult,
            QStringLiteral("unsupported_result"),
            {},
        },
        {
            QByteArrayLiteral("function process(context) return { 1, 2, 3 } end\n"),
            ProcessorExecutionState::OutputLimitExceeded,
            QStringLiteral("result_limit_exceeded"),
            smallEntryLimit,
        },
        {
            QByteArrayLiteral("function process(context) return { { { 1 } } } end\n"),
            ProcessorExecutionState::OutputLimitExceeded,
            QStringLiteral("result_limit_exceeded"),
            shallowLimit,
        },
    };

    for (const Case &testCase : cases) {
        MessageProcessorEngine engine({luaAdapter()});
        const ProcessorExecutionResult result = engine.execute(
            luaRevision(testCase.source),
            ProcessorRuntimeConformance::context(),
            testCase.limits);
        QCOMPARE(static_cast<int>(result.state), static_cast<int>(testCase.state));
        QVERIFY2(diagnosticByCode(result.diagnostics, testCase.code),
            qPrintable(QStringLiteral("Missing diagnostic %1").arg(testCase.code)));
    }
}

void LuaRuntimeAdapterTest::ignoresResultMetatablesAndHidesUnsafeLibraries()
{
    const ProcessorRevisionSnapshot revision = luaRevision(QByteArrayLiteral(
        "function process(context)\n"
        "    local result = {\n"
        "        ioHidden = io == nil,\n"
        "        osHidden = os == nil,\n"
        "        debugHidden = debug == nil,\n"
        "        packageHidden = package == nil,\n"
        "        requireHidden = require == nil,\n"
        "        loadHidden = load == nil,\n"
        "        collectGarbageHidden = collectgarbage == nil,\n"
        "    }\n"
        "    setmetatable(result, { __index = function() error('metatable lookup') end })\n"
        "    return result\n"
        "end\n"));
    MessageProcessorEngine engine({luaAdapter()});

    const ProcessorExecutionResult result = engine.execute(
        revision,
        ProcessorRuntimeConformance::context());
    QVERIFY2(result.succeeded(), qPrintable(result.diagnostics.value(0).message));
    const QCborMap map = result.value.toMap();
    QCOMPARE(map.size(), 7);
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        QCOMPARE(it.value().toBool(), true);
    }
}

QTEST_GUILESS_MAIN(LuaRuntimeAdapterTest)

#include "test_luaruntimeadapter.moc"
