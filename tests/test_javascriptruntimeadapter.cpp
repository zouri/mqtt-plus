#include "services/processors/messageprocessorengine.h"
#include "services/processors/runtimes/javascriptruntimeadapter.h"
#include "support/processorruntimeconformance.h"

#include <QtTest/QtTest>

#include <QCborMap>
#include <QCryptographicHash>

#include <algorithm>
#include <utility>

namespace {

ProcessorRevisionSnapshot javascriptRevision(
    const QByteArray &entrySource,
    QVector<ProcessorSourceFile> helperFiles = {},
    const QString &entrySymbol = QStringLiteral("process"))
{
    ProcessorSourceFile entryFile;
    entryFile.path = QStringLiteral("main.js");
    entryFile.mediaType = QStringLiteral("text/javascript");
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
    revision.id = QStringLiteral("javascript-revision-%1").arg(contentHash.left(12));
    revision.processorId = QStringLiteral("javascript-processor");
    revision.revisionNumber = 1;
    revision.contractId = QStringLiteral("mqtt-plus.message-processor/v1");
    revision.languageId = QStringLiteral("javascript");
    revision.runtimeId = QStringLiteral("qt-qjs");
    revision.entryFile = entryFile.path;
    revision.entrySymbol = entrySymbol;
    revision.contentHash = contentHash;
    revision.files = std::move(helperFiles);
    revision.createdAt = QStringLiteral("2026-08-05T08:00:00.000Z");
    return revision;
}

QSharedPointer<JavaScriptRuntimeAdapter> javascriptAdapter()
{
    return QSharedPointer<JavaScriptRuntimeAdapter>::create();
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

class JavaScriptRuntimeAdapterTest : public QObject
{
    Q_OBJECT

private slots:
    void describesTheJavaScriptRuntime();
    void passesCommonRuntimeConformanceHarness();
    void isolatesStateAndRecoversAfterRuntimeErrors();
    void timesOutAndRecoversForTheNextMessage();
    void loadsMultipleFilesBeforeTheEntryFile();
    void rejectsInvalidSourceDuringPreparation();
    void rejectsHostileAndOversizedResults();
    void supportsByteViewsAndRejectsAsyncResults();
    void distinguishesThrownValuesFromReturnedErrorObjects();
    void interruptsHostileResultInspectionAndRecovers();
    void exposesNoQtApplicationOrHostGlobals();
};

void JavaScriptRuntimeAdapterTest::describesTheJavaScriptRuntime()
{
    const RuntimeDescriptor descriptor = javascriptAdapter()->descriptor();
    QCOMPARE(descriptor.runtimeId, QStringLiteral("qt-qjs"));
    QCOMPARE(descriptor.languageId, QStringLiteral("javascript"));
    QVERIFY(descriptor.runtimeVersion.startsWith(QStringLiteral("Qt 6.11")));
    QVERIFY(descriptor.supportedContractIds.contains(
        QStringLiteral("mqtt-plus.message-processor/v1")));
    QCOMPARE(
        static_cast<int>(descriptor.executionMode),
        static_cast<int>(RuntimeExecutionMode::ParseWorkerThread));
}

void JavaScriptRuntimeAdapterTest::passesCommonRuntimeConformanceHarness()
{
    const ProcessorRevisionSnapshot revision = javascriptRevision(QByteArrayLiteral(
        "function process(context) {\n"
        "    return {\n"
        "        context: {\n"
        "            topic: context.topic,\n"
        "            payload: context.payload,\n"
        "            receivedAt: context.receivedAt,\n"
        "            format: context.format,\n"
        "            decoded: context.decoded,\n"
        "            decodeError: context.decodeError,\n"
        "            parameters: context.parameters,\n"
        "        },\n"
        "        values: [null, true, 42, 3.5, 'value', new Uint8Array([97, 98, 99])],\n"
        "    }\n"
        "}\n"));

    ProcessorRuntimeConformance::verify(javascriptAdapter(), revision);
}

void JavaScriptRuntimeAdapterTest::isolatesStateAndRecoversAfterRuntimeErrors()
{
    const ProcessorRevisionSnapshot revision = javascriptRevision(QByteArrayLiteral(
        "let calls = 0\n"
        "function process(context) {\n"
        "    calls += 1\n"
        "    if (context.topic === 'bad') throw new Error('bad payload')\n"
        "    return calls\n"
        "}\n"));
    MessageProcessorEngine engine({javascriptAdapter()});

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
    QVERIFY(failure->file.endsWith(QStringLiteral("main.js")));
    QVERIFY(failure->line > 0);

    context.topic = QStringLiteral("good");
    const ProcessorExecutionResult recovered = engine.execute(revision, context);
    QVERIFY2(recovered.succeeded(), qPrintable(recovered.diagnostics.value(0).message));
    QCOMPARE(recovered.value.toInteger(), qint64(1));
}

void JavaScriptRuntimeAdapterTest::timesOutAndRecoversForTheNextMessage()
{
    const ProcessorRevisionSnapshot revision = javascriptRevision(QByteArrayLiteral(
        "function process(context) {\n"
        "    if (context.topic === 'timeout') while (true) {}\n"
        "    return 'ok'\n"
        "}\n"));
    MessageProcessorEngine engine({javascriptAdapter()});
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

void JavaScriptRuntimeAdapterTest::loadsMultipleFilesBeforeTheEntryFile()
{
    ProcessorSourceFile helper;
    helper.path = QStringLiteral("lib/protocol.js");
    helper.mediaType = QStringLiteral("text/javascript");
    helper.content = QByteArrayLiteral(
        "function applyScale(value, scale) { return value * scale }\n");
    const ProcessorRevisionSnapshot revision = javascriptRevision(
        QByteArrayLiteral(
            "function transform(context) {\n"
            "    return applyScale(4, context.parameters.scale)\n"
            "}\n"),
        {helper},
        QStringLiteral("transform"));
    MessageProcessorEngine engine({javascriptAdapter()});

    const ProcessorExecutionResult result = engine.execute(
        revision,
        ProcessorRuntimeConformance::context());
    QVERIFY2(result.succeeded(), qPrintable(result.diagnostics.value(0).message));
    QCOMPARE(result.value.toInteger(), qint64(40));
}

void JavaScriptRuntimeAdapterTest::rejectsInvalidSourceDuringPreparation()
{
    MessageProcessorEngine engine({javascriptAdapter()});

    const ProcessorRevisionSnapshot syntaxError = javascriptRevision(QByteArrayLiteral(
        "function process(context) {\n"
        "    return {\n"
        "}\n"));
    const ProcessorValidationResult syntaxResult = engine.validate(syntaxError);
    QCOMPARE(
        static_cast<int>(syntaxResult.state),
        static_cast<int>(ProcessorValidationState::InvalidSource));
    const ProcessorDiagnostic *syntaxDiagnostic = diagnosticByCode(
        syntaxResult.diagnostics,
        QStringLiteral("invalid_source"));
    QVERIFY(syntaxDiagnostic);
    QVERIFY(syntaxDiagnostic->file.endsWith(QStringLiteral("main.js")));
    QVERIFY2(syntaxDiagnostic->line > 0, qPrintable(syntaxDiagnostic->message));

    const ProcessorRevisionSnapshot missingEntry = javascriptRevision(QByteArrayLiteral(
        "function anotherFunction(context) { return 1 }\n"));
    const ProcessorValidationResult missingResult = engine.validate(missingEntry);
    QCOMPARE(
        static_cast<int>(missingResult.state),
        static_cast<int>(ProcessorValidationState::InvalidSource));
    QVERIFY(diagnosticByCode(
        missingResult.diagnostics,
        QStringLiteral("invalid_source")));

    const ProcessorRevisionSnapshot topLevelFailure = javascriptRevision(QByteArrayLiteral(
        "throw 'top-level failure'\n"
        "function process(context) { return 1 }\n"));
    const ProcessorValidationResult preparationResult = engine.validate(topLevelFailure);
    QCOMPARE(
        static_cast<int>(preparationResult.state),
        static_cast<int>(ProcessorValidationState::PreparationFailed));
    QVERIFY(diagnosticByCode(
        preparationResult.diagnostics,
        QStringLiteral("preparation_failed")));
}

void JavaScriptRuntimeAdapterTest::rejectsHostileAndOversizedResults()
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
            QByteArrayLiteral("function process(context) { return function() {} }\n"),
            ProcessorExecutionState::UnsupportedResult,
            QStringLiteral("unsupported_result"),
            {},
        },
        {
            QByteArrayLiteral(
                "function process(context) { const value = {}; value.self = value; return value }\n"),
            ProcessorExecutionState::UnsupportedResult,
            QStringLiteral("unsupported_result"),
            {},
        },
        {
            QByteArrayLiteral(
                "function process(context) { return { get value() { throw new Error('getter ran') } } }\n"),
            ProcessorExecutionState::UnsupportedResult,
            QStringLiteral("unsupported_result"),
            {},
        },
        {
            QByteArrayLiteral("function process(context) { return [1, 2, 3] }\n"),
            ProcessorExecutionState::OutputLimitExceeded,
            QStringLiteral("result_limit_exceeded"),
            smallEntryLimit,
        },
        {
            QByteArrayLiteral("function process(context) { return [[[1]]] }\n"),
            ProcessorExecutionState::OutputLimitExceeded,
            QStringLiteral("result_limit_exceeded"),
            shallowLimit,
        },
    };

    for (const Case &testCase : cases) {
        MessageProcessorEngine engine({javascriptAdapter()});
        const ProcessorExecutionResult result = engine.execute(
            javascriptRevision(testCase.source),
            ProcessorRuntimeConformance::context(),
            testCase.limits);
        QCOMPARE(static_cast<int>(result.state), static_cast<int>(testCase.state));
        QVERIFY2(diagnosticByCode(result.diagnostics, testCase.code),
            qPrintable(QStringLiteral("Missing diagnostic %1").arg(testCase.code)));
        for (const ProcessorDiagnostic &diagnostic : result.diagnostics) {
            QVERIFY(!diagnostic.message.contains(QStringLiteral("getter ran")));
        }
    }
}

void JavaScriptRuntimeAdapterTest::supportsByteViewsAndRejectsAsyncResults()
{
    MessageProcessorEngine engine({javascriptAdapter()});
    const ProcessorRevisionSnapshot bytesRevision = javascriptRevision(QByteArrayLiteral(
        "function process(context) {\n"
        "    const source = new Uint8Array([9, 1, 2, 3, 8])\n"
        "    return {\n"
        "        buffer: source.buffer,\n"
        "        payload: context.payload,\n"
        "        view: new Uint8Array(source.buffer, 1, 3),\n"
        "    }\n"
        "}\n"));
    const ProcessorExecutionResult bytesResult = engine.execute(
        bytesRevision,
        ProcessorRuntimeConformance::context());
    QVERIFY2(bytesResult.succeeded(), qPrintable(bytesResult.diagnostics.value(0).message));
    const QCborMap bytes = bytesResult.value.toMap();
    QCOMPARE(bytes.value(QStringLiteral("buffer")).toByteArray(), QByteArray::fromHex("0901020308"));
    QCOMPARE(bytes.value(QStringLiteral("payload")).toByteArray(), QByteArray::fromHex("00017f80ff"));
    QCOMPARE(bytes.value(QStringLiteral("view")).toByteArray(), QByteArray::fromHex("010203"));

    const ProcessorExecutionResult promiseResult = engine.execute(
        javascriptRevision(QByteArrayLiteral(
            "function process(context) { return Promise.resolve(1) }\n")),
        ProcessorRuntimeConformance::context());
    QCOMPARE(
        static_cast<int>(promiseResult.state),
        static_cast<int>(ProcessorExecutionState::UnsupportedResult));
    QVERIFY(diagnosticByCode(
        promiseResult.diagnostics,
        QStringLiteral("unsupported_result")));
}

void JavaScriptRuntimeAdapterTest::distinguishesThrownValuesFromReturnedErrorObjects()
{
    MessageProcessorEngine engine({javascriptAdapter()});
    const ProcessorRevisionSnapshot revision = javascriptRevision(QByteArrayLiteral(
        "function process(context) {\n"
        "    if (context.topic === 'throw') throw 'primitive failure'\n"
        "    return new Error('returned value')\n"
        "}\n"));

    MessageProcessorContext context = ProcessorRuntimeConformance::context();
    context.topic = QStringLiteral("throw");
    const ProcessorExecutionResult thrown = engine.execute(revision, context);
    QCOMPARE(
        static_cast<int>(thrown.state),
        static_cast<int>(ProcessorExecutionState::ExecutionFailed));
    const ProcessorDiagnostic *failure = diagnosticByCode(
        thrown.diagnostics,
        QStringLiteral("execution_failed"));
    QVERIFY(failure);
    QVERIFY2(
        failure->message.contains(QStringLiteral("primitive failure")),
        qPrintable(failure->message));

    context.topic = QStringLiteral("return");
    const ProcessorExecutionResult returned = engine.execute(revision, context);
    QCOMPARE(
        static_cast<int>(returned.state),
        static_cast<int>(ProcessorExecutionState::UnsupportedResult));
    QVERIFY(diagnosticByCode(
        returned.diagnostics,
        QStringLiteral("unsupported_result")));
}

void JavaScriptRuntimeAdapterTest::interruptsHostileResultInspectionAndRecovers()
{
    MessageProcessorEngine engine({javascriptAdapter()});
    const ProcessorRevisionSnapshot revision = javascriptRevision(QByteArrayLiteral(
        "function process(context) {\n"
        "    if (context.topic !== 'timeout') return 'ok'\n"
        "    return new Proxy({}, { ownKeys() { while (true) {} } })\n"
        "}\n"));
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

void JavaScriptRuntimeAdapterTest::exposesNoQtApplicationOrHostGlobals()
{
    const ProcessorRevisionSnapshot revision = javascriptRevision(
        QByteArrayLiteral(
        "function transform(context) {\n"
        "    return {\n"
        "        Qt: typeof Qt,\n"
        "        app: typeof app,\n"
        "        console: typeof console,\n"
        "        gc: typeof gc,\n"
        "        print: typeof print,\n"
        "        process: typeof process,\n"
        "        require: typeof require,\n"
        "        XMLHttpRequest: typeof XMLHttpRequest,\n"
        "    }\n"
        "}\n"),
        {},
        QStringLiteral("transform"));
    MessageProcessorEngine engine({javascriptAdapter()});

    const ProcessorExecutionResult result = engine.execute(
        revision,
        ProcessorRuntimeConformance::context());
    QVERIFY2(result.succeeded(), qPrintable(result.diagnostics.value(0).message));
    const QCborMap globals = result.value.toMap();
    QCOMPARE(globals.size(), 8);
    for (auto it = globals.constBegin(); it != globals.constEnd(); ++it) {
        QCOMPARE(it.value().toString(), QStringLiteral("undefined"));
    }
}

QTEST_GUILESS_MAIN(JavaScriptRuntimeAdapterTest)

#include "test_javascriptruntimeadapter.moc"
