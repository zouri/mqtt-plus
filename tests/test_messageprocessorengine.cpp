#include "services/processors/messageprocessorengine.h"
#include "services/processors/processorruntimeadapter.h"
#include "services/processors/processorruntimeregistry.h"
#include "support/processorruntimeconformance.h"

#include <QtTest/QtTest>

#include <QCborArray>
#include <QCborMap>

#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

class FakePreparedProcessor : public PreparedProcessor
{
public:
    explicit FakePreparedProcessor(QString sourceContentHash)
        : contentHash(std::move(sourceContentHash))
    {
    }

    QString contentHash;
};

class FakeRuntimeAdapter : public ProcessorRuntimeAdapter
{
public:
    explicit FakeRuntimeAdapter(RuntimeDescriptor sourceDescriptor)
        : runtimeDescriptor(std::move(sourceDescriptor))
    {
        resultFactory = [](const MessageProcessorContext &context) {
            ProcessorExecutionResult result;
            result.state = ProcessorExecutionState::Succeeded;
            result.value = ProcessorRuntimeConformance::expectedValue(context);
            return result;
        };
    }

    RuntimeDescriptor descriptor() const override
    {
        if (throwDuringDescriptor) {
            throw std::runtime_error("fake descriptor failure");
        }
        return runtimeDescriptor;
    }

    ProcessorPreparationResult prepare(
        const ProcessorRevisionSnapshot &revision,
        const ProcessorExecutionLimits &limits) override
    {
        ++prepareCalls;
        lastPreparedRevision = revision;
        lastPreparationLimits = limits;
        if (throwDuringPreparation) {
            throw std::runtime_error("fake prepare failure");
        }

        ProcessorPreparationResult result;
        result.state = preparationState;
        result.diagnostics = preparationDiagnostics;
        if (preparationState == ProcessorValidationState::Ready && returnPreparedHandle) {
            result.prepared = QSharedPointer<FakePreparedProcessor>::create(
                revision.contentHash);
        }
        return result;
    }

    ProcessorExecutionResult execute(
        const PreparedProcessorHandle &prepared,
        const MessageProcessorContext &context,
        const ProcessorExecutionLimits &limits) override
    {
        ++executeCalls;
        lastPreparedHandle = prepared;
        lastContext = context;
        lastExecutionLimits = limits;
        if (throwDuringExecution) {
            throw std::runtime_error("fake execute failure");
        }
        return resultFactory(context);
    }

    RuntimeDescriptor runtimeDescriptor;
    ProcessorValidationState preparationState = ProcessorValidationState::Ready;
    QVector<ProcessorDiagnostic> preparationDiagnostics;
    std::function<ProcessorExecutionResult(const MessageProcessorContext &)> resultFactory;
    bool returnPreparedHandle = true;
    bool throwDuringDescriptor = false;
    bool throwDuringPreparation = false;
    bool throwDuringExecution = false;
    int prepareCalls = 0;
    int executeCalls = 0;
    ProcessorRevisionSnapshot lastPreparedRevision;
    PreparedProcessorHandle lastPreparedHandle;
    MessageProcessorContext lastContext;
    ProcessorExecutionLimits lastPreparationLimits;
    ProcessorExecutionLimits lastExecutionLimits;
};

RuntimeDescriptor descriptor(
    const QString &runtimeId,
    const QString &languageId)
{
    RuntimeDescriptor result;
    result.runtimeId = runtimeId;
    result.languageId = languageId;
    result.displayName = runtimeId;
    result.runtimeVersion = QStringLiteral("test-1");
    result.supportedContractIds = {
        QStringLiteral("mqtt-plus.message-processor/v1"),
    };
    result.sourceExtensions = {
        languageId == QStringLiteral("lua")
            ? QStringLiteral("lua")
            : QStringLiteral("js"),
    };
    return result;
}

ProcessorRevisionSnapshot revision(
    const QString &runtimeId = QStringLiteral("qt-qjs"),
    const QString &languageId = QStringLiteral("javascript"),
    const QString &contentHash = QString(64, QLatin1Char('a')))
{
    ProcessorRevisionSnapshot result;
    result.id = QStringLiteral("revision-1");
    result.processorId = QStringLiteral("processor-1");
    result.revisionNumber = 1;
    result.contractId = QStringLiteral("mqtt-plus.message-processor/v1");
    result.languageId = languageId;
    result.runtimeId = runtimeId;
    result.entryFile = languageId == QStringLiteral("lua")
        ? QStringLiteral("main.lua")
        : QStringLiteral("main.js");
    result.entrySymbol = QStringLiteral("process");
    result.contentHash = contentHash;
    result.files = {
        {
            result.entryFile,
            QStringLiteral("text/plain"),
            QByteArrayLiteral("test source"),
            QString(64, QLatin1Char('b')),
        },
    };
    result.createdAt = QStringLiteral("2026-08-05T08:00:00.000Z");
    return result;
}

bool hasDiagnostic(
    const QVector<ProcessorDiagnostic> &diagnostics,
    const QString &code)
{
    return std::any_of(
        diagnostics.cbegin(),
        diagnostics.cend(),
        [&code](const ProcessorDiagnostic &diagnostic) {
            return diagnostic.code == code;
        });
}

qsizetype diagnosticsSize(const QVector<ProcessorDiagnostic> &diagnostics)
{
    qsizetype result = 0;
    for (const ProcessorDiagnostic &diagnostic : diagnostics) {
        result += diagnostic.code.toUtf8().size();
        result += diagnostic.message.toUtf8().size();
        result += diagnostic.file.toUtf8().size();
    }
    return result;
}

ProcessorExecutionResult successfulResult(const QCborValue &value)
{
    ProcessorExecutionResult result;
    result.state = ProcessorExecutionState::Succeeded;
    result.value = value;
    return result;
}

} // namespace

class MessageProcessorEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void registryRejectsInvalidAndDuplicateDescriptors();
    void selectsRuntimeByStableIdAndCachesImmutableContent();
    void reportsRuntimeCompatibilityFailures();
    void containsPreparationAndExecutionFailures();
    void enforcesNormalizedResultLimits();
    void boundsDiagnosticsAndPreview();
    void preservesStringPreviewText();
    void passesCommonRuntimeConformanceHarness();
};

void MessageProcessorEngineTest::registryRejectsInvalidAndDuplicateDescriptors()
{
    ProcessorRuntimeRegistry registry;
    QString error;
    QVERIFY(!registry.addAdapter({}, &error));
    QVERIFY(!error.isEmpty());

    RuntimeDescriptor invalidDescriptor = descriptor(
        QStringLiteral("invalid-runtime"),
        QStringLiteral("javascript"));
    invalidDescriptor.runtimeVersion.clear();
    const auto invalidAdapter = QSharedPointer<FakeRuntimeAdapter>::create(invalidDescriptor);
    QVERIFY(!registry.addAdapter(invalidAdapter, &error));
    QVERIFY(error.contains(QStringLiteral("version"), Qt::CaseInsensitive));

    const auto javascriptAdapter = QSharedPointer<FakeRuntimeAdapter>::create(
        descriptor(QStringLiteral("qt-qjs"), QStringLiteral("javascript")));
    const auto luaAdapter = QSharedPointer<FakeRuntimeAdapter>::create(
        descriptor(QStringLiteral("lua-5.5"), QStringLiteral("lua")));
    QVERIFY2(registry.addAdapter(javascriptAdapter, &error), qPrintable(error));
    QVERIFY2(registry.addAdapter(luaAdapter, &error), qPrintable(error));
    QVERIFY(!registry.addAdapter(javascriptAdapter, &error));
    QVERIFY(error.contains(QStringLiteral("already"), Qt::CaseInsensitive));

    const QVector<RuntimeDescriptor> descriptors = registry.descriptors();
    QCOMPARE(descriptors.size(), 2);
    QCOMPARE(descriptors.at(0).runtimeId, QStringLiteral("lua-5.5"));
    QCOMPARE(descriptors.at(1).runtimeId, QStringLiteral("qt-qjs"));

    MessageProcessorEngine invalidEngine({javascriptAdapter, javascriptAdapter});
    const ProcessorExecutionResult invalidEngineResult = invalidEngine.execute(
        revision(),
        ProcessorRuntimeConformance::context());
    QCOMPARE(
        static_cast<int>(invalidEngineResult.state),
        static_cast<int>(ProcessorExecutionState::InternalError));
    QVERIFY(hasDiagnostic(
        invalidEngineResult.diagnostics,
        QStringLiteral("internal_runtime_error")));

    const auto throwingAdapter = QSharedPointer<FakeRuntimeAdapter>::create(
        descriptor(QStringLiteral("throwing-runtime"), QStringLiteral("javascript")));
    throwingAdapter->throwDuringDescriptor = true;
    MessageProcessorEngine throwingEngine({throwingAdapter});
    const ProcessorValidationResult throwingEngineResult = throwingEngine.validate(revision());
    QCOMPARE(
        static_cast<int>(throwingEngineResult.state),
        static_cast<int>(ProcessorValidationState::InternalError));
    QVERIFY(hasDiagnostic(
        throwingEngineResult.diagnostics,
        QStringLiteral("internal_runtime_error")));
}

void MessageProcessorEngineTest::selectsRuntimeByStableIdAndCachesImmutableContent()
{
    const auto javascriptAdapter = QSharedPointer<FakeRuntimeAdapter>::create(
        descriptor(QStringLiteral("qt-qjs"), QStringLiteral("javascript")));
    const auto otherAdapter = QSharedPointer<FakeRuntimeAdapter>::create(
        descriptor(QStringLiteral("other-js"), QStringLiteral("javascript")));
    MessageProcessorEngine engine({otherAdapter, javascriptAdapter}, 2);

    ProcessorExecutionLimits limits;
    limits.wallTimeMilliseconds = 5000;
    const MessageProcessorContext context = ProcessorRuntimeConformance::context();
    ProcessorRevisionSnapshot first = revision();
    const ProcessorExecutionResult firstResult = engine.execute(first, context, limits);
    QVERIFY(firstResult.succeeded());
    QCOMPARE(javascriptAdapter->prepareCalls, 1);
    QCOMPARE(javascriptAdapter->executeCalls, 1);
    QCOMPARE(otherAdapter->prepareCalls, 0);
    QCOMPARE(
        javascriptAdapter->lastPreparationLimits.wallTimeMilliseconds,
        ProcessorExecutionLimits::kMaximumWallTimeMilliseconds);

    ProcessorRevisionSnapshot sameContent = first;
    sameContent.id = QStringLiteral("revision-2");
    sameContent.revisionNumber = 2;
    QVERIFY(engine.execute(sameContent, context).succeeded());
    QCOMPARE(javascriptAdapter->prepareCalls, 1);
    QCOMPARE(javascriptAdapter->executeCalls, 2);

    ProcessorRevisionSnapshot secondContent = sameContent;
    secondContent.id = QStringLiteral("revision-3");
    secondContent.revisionNumber = 3;
    secondContent.contentHash = QString(64, QLatin1Char('c'));
    QVERIFY(engine.validate(secondContent).isReady());
    QVERIFY(engine.execute(secondContent, context).succeeded());
    QCOMPARE(javascriptAdapter->prepareCalls, 2);
    QCOMPARE(javascriptAdapter->executeCalls, 3);

    ProcessorRevisionSnapshot thirdContent = secondContent;
    thirdContent.id = QStringLiteral("revision-4");
    thirdContent.revisionNumber = 4;
    thirdContent.contentHash = QString(64, QLatin1Char('d'));
    QVERIFY(engine.execute(thirdContent, context).succeeded());
    QCOMPARE(javascriptAdapter->prepareCalls, 3);

    QVERIFY(engine.execute(first, context).succeeded());
    QCOMPARE(javascriptAdapter->prepareCalls, 4);
}

void MessageProcessorEngineTest::reportsRuntimeCompatibilityFailures()
{
    const auto adapter = QSharedPointer<FakeRuntimeAdapter>::create(
        descriptor(QStringLiteral("qt-qjs"), QStringLiteral("javascript")));
    MessageProcessorEngine engine({adapter});

    ProcessorRevisionSnapshot missingRuntime = revision();
    missingRuntime.runtimeId = QStringLiteral("missing-runtime");
    const ProcessorExecutionResult missingResult = engine.execute(
        missingRuntime,
        ProcessorRuntimeConformance::context());
    QCOMPARE(
        static_cast<int>(missingResult.state),
        static_cast<int>(ProcessorExecutionState::RuntimeUnavailable));
    QVERIFY(hasDiagnostic(missingResult.diagnostics, QStringLiteral("runtime_unavailable")));

    ProcessorRevisionSnapshot wrongLanguage = revision();
    wrongLanguage.languageId = QStringLiteral("lua");
    wrongLanguage.entryFile = QStringLiteral("main.lua");
    wrongLanguage.files.first().path = wrongLanguage.entryFile;
    const ProcessorValidationResult languageResult = engine.validate(wrongLanguage);
    QCOMPARE(
        static_cast<int>(languageResult.state),
        static_cast<int>(ProcessorValidationState::RuntimeUnavailable));
    QVERIFY(hasDiagnostic(languageResult.diagnostics, QStringLiteral("runtime_unavailable")));

    ProcessorRevisionSnapshot wrongContract = revision();
    wrongContract.contractId = QStringLiteral("mqtt-plus.message-processor/v2");
    const ProcessorValidationResult contractResult = engine.validate(wrongContract);
    QCOMPARE(
        static_cast<int>(contractResult.state),
        static_cast<int>(ProcessorValidationState::RuntimeUnavailable));
    QVERIFY(hasDiagnostic(contractResult.diagnostics, QStringLiteral("contract_unsupported")));

    ProcessorRevisionSnapshot invalidRevision = revision();
    invalidRevision.entryFile = QStringLiteral("missing.js");
    const ProcessorValidationResult invalidResult = engine.validate(invalidRevision);
    QCOMPARE(
        static_cast<int>(invalidResult.state),
        static_cast<int>(ProcessorValidationState::InvalidSource));
    QVERIFY(hasDiagnostic(invalidResult.diagnostics, QStringLiteral("invalid_source")));
    QCOMPARE(adapter->prepareCalls, 0);
}

void MessageProcessorEngineTest::containsPreparationAndExecutionFailures()
{
    const auto adapter = QSharedPointer<FakeRuntimeAdapter>::create(
        descriptor(QStringLiteral("qt-qjs"), QStringLiteral("javascript")));
    MessageProcessorEngine engine({adapter});

    adapter->preparationState = ProcessorValidationState::InvalidSource;
    adapter->preparationDiagnostics.append({
        QStringLiteral("preparation_warning"),
        QStringLiteral("Warning emitted before the failure."),
        {},
        -1,
        -1,
    });
    const ProcessorValidationResult invalidSource = engine.validate(revision());
    QCOMPARE(
        static_cast<int>(invalidSource.state),
        static_cast<int>(ProcessorValidationState::InvalidSource));
    QVERIFY(hasDiagnostic(invalidSource.diagnostics, QStringLiteral("invalid_source")));

    adapter->preparationState = ProcessorValidationState::Ready;
    adapter->preparationDiagnostics.clear();
    adapter->throwDuringPreparation = true;
    ProcessorRevisionSnapshot preparationException = revision(
        QStringLiteral("qt-qjs"),
        QStringLiteral("javascript"),
        QString(64, QLatin1Char('c')));
    const ProcessorExecutionResult preparationFailure = engine.execute(
        preparationException,
        ProcessorRuntimeConformance::context());
    QCOMPARE(
        static_cast<int>(preparationFailure.state),
        static_cast<int>(ProcessorExecutionState::InternalError));
    QVERIFY(hasDiagnostic(
        preparationFailure.diagnostics,
        QStringLiteral("internal_runtime_error")));

    adapter->throwDuringPreparation = false;
    adapter->throwDuringExecution = true;
    ProcessorRevisionSnapshot executionException = preparationException;
    executionException.id = QStringLiteral("revision-execution-exception");
    executionException.revisionNumber = 2;
    executionException.contentHash = QString(64, QLatin1Char('d'));
    const ProcessorExecutionResult executionFailure = engine.execute(
        executionException,
        ProcessorRuntimeConformance::context());
    QCOMPARE(
        static_cast<int>(executionFailure.state),
        static_cast<int>(ProcessorExecutionState::InternalError));
    QVERIFY(hasDiagnostic(
        executionFailure.diagnostics,
        QStringLiteral("internal_runtime_error")));

    adapter->throwDuringExecution = false;
    adapter->preparationDiagnostics.append({
        QStringLiteral("preparation_warning"),
        QStringLiteral("Prepared with a warning."),
        {},
        -1,
        -1,
    });
    adapter->resultFactory = [](const MessageProcessorContext &) {
        ProcessorExecutionResult result;
        result.state = ProcessorExecutionState::ExecutionFailed;
        return result;
    };
    ProcessorRevisionSnapshot reportedFailureRevision = executionException;
    reportedFailureRevision.id = QStringLiteral("revision-reported-failure");
    reportedFailureRevision.revisionNumber = 3;
    reportedFailureRevision.contentHash = QString(64, QLatin1Char('e'));
    const ProcessorExecutionResult reportedFailure = engine.execute(
        reportedFailureRevision,
        ProcessorRuntimeConformance::context());
    QCOMPARE(
        static_cast<int>(reportedFailure.state),
        static_cast<int>(ProcessorExecutionState::ExecutionFailed));
    QVERIFY(hasDiagnostic(reportedFailure.diagnostics, QStringLiteral("execution_failed")));
    QVERIFY(hasDiagnostic(
        reportedFailure.diagnostics,
        QStringLiteral("preparation_warning")));
}

void MessageProcessorEngineTest::enforcesNormalizedResultLimits()
{
    const auto adapter = QSharedPointer<FakeRuntimeAdapter>::create(
        descriptor(QStringLiteral("qt-qjs"), QStringLiteral("javascript")));
    MessageProcessorEngine engine({adapter});
    const ProcessorRevisionSnapshot sourceRevision = revision();
    const MessageProcessorContext context = ProcessorRuntimeConformance::context();

    adapter->resultFactory = [](const MessageProcessorContext &) {
        QCborMap map;
        map.insert(1, QStringLiteral("invalid key"));
        return successfulResult(map);
    };
    ProcessorExecutionResult result = engine.execute(sourceRevision, context);
    QCOMPARE(
        static_cast<int>(result.state),
        static_cast<int>(ProcessorExecutionState::UnsupportedResult));
    QVERIFY(hasDiagnostic(result.diagnostics, QStringLiteral("unsupported_result")));

    adapter->resultFactory = [](const MessageProcessorContext &) {
        return successfulResult(std::numeric_limits<double>::infinity());
    };
    result = engine.execute(sourceRevision, context);
    QCOMPARE(
        static_cast<int>(result.state),
        static_cast<int>(ProcessorExecutionState::UnsupportedResult));

    adapter->resultFactory = [](const MessageProcessorContext &) {
        QCborArray inner;
        inner.append(1);
        QCborArray middle;
        middle.append(inner);
        QCborArray outer;
        outer.append(middle);
        return successfulResult(outer);
    };
    ProcessorExecutionLimits depthLimits;
    depthLimits.maxResultDepth = 1;
    result = engine.execute(sourceRevision, context, depthLimits);
    QCOMPARE(
        static_cast<int>(result.state),
        static_cast<int>(ProcessorExecutionState::OutputLimitExceeded));
    QVERIFY(hasDiagnostic(result.diagnostics, QStringLiteral("result_limit_exceeded")));

    adapter->resultFactory = [](const MessageProcessorContext &) {
        QCborArray array;
        array.append(1);
        array.append(2);
        array.append(3);
        return successfulResult(array);
    };
    ProcessorExecutionLimits entryLimits;
    entryLimits.maxCollectionEntries = 2;
    result = engine.execute(sourceRevision, context, entryLimits);
    QCOMPARE(
        static_cast<int>(result.state),
        static_cast<int>(ProcessorExecutionState::OutputLimitExceeded));

    adapter->resultFactory = [](const MessageProcessorContext &) {
        return successfulResult(QByteArray(1024, 'x'));
    };
    ProcessorExecutionLimits byteLimits;
    byteLimits.maxResultBytes = 16;
    result = engine.execute(sourceRevision, context, byteLimits);
    QCOMPARE(
        static_cast<int>(result.state),
        static_cast<int>(ProcessorExecutionState::OutputLimitExceeded));
    QVERIFY(result.value.isUndefined());
    QVERIFY(result.preview.isEmpty());
}

void MessageProcessorEngineTest::boundsDiagnosticsAndPreview()
{
    const auto adapter = QSharedPointer<FakeRuntimeAdapter>::create(
        descriptor(QStringLiteral("qt-qjs"), QStringLiteral("javascript")));
    MessageProcessorEngine engine({adapter});
    const ProcessorRevisionSnapshot sourceRevision = revision();
    const MessageProcessorContext context = ProcessorRuntimeConformance::context();

    adapter->resultFactory = [](const MessageProcessorContext &) {
        ProcessorExecutionResult result;
        result.state = ProcessorExecutionState::ExecutionFailed;
        result.diagnostics.append({
            QStringLiteral("INVALID CODE"),
            QString(500, QLatin1Char('m')),
            QString(500, QLatin1Char('f')),
            10,
            20,
        });
        return result;
    };
    ProcessorExecutionLimits diagnosticLimits;
    diagnosticLimits.maxDiagnosticsBytes = 128;
    ProcessorExecutionResult result = engine.execute(
        sourceRevision,
        context,
        diagnosticLimits);
    QVERIFY(diagnosticsSize(result.diagnostics) <= diagnosticLimits.maxDiagnosticsBytes);
    QVERIFY(!result.diagnostics.isEmpty());
    QCOMPARE(result.diagnostics.first().code, QStringLiteral("execution_failed"));
    QVERIFY(hasDiagnostic(result.diagnostics, QStringLiteral("diagnostics_truncated")));
    QVERIFY(hasDiagnostic(result.diagnostics, QStringLiteral("internal_runtime_error")));

    adapter->resultFactory = [](const MessageProcessorContext &) {
        return successfulResult(QString(1000, QLatin1Char('p')));
    };
    ProcessorExecutionLimits previewLimits;
    previewLimits.maxResultBytes = 4096;
    previewLimits.maxPreviewCharacters = 12;
    result = engine.execute(sourceRevision, context, previewLimits);
    QVERIFY(result.succeeded());
    QCOMPARE(result.preview.size(), previewLimits.maxPreviewCharacters);
}

void MessageProcessorEngineTest::preservesStringPreviewText()
{
    const auto adapter = QSharedPointer<FakeRuntimeAdapter>::create(
        descriptor(QStringLiteral("qt-qjs"), QStringLiteral("javascript")));
    MessageProcessorEngine engine({adapter});
    const QString displayText = QStringLiteral("消息类型: 操作台控制量\n消息ID: 8");
    adapter->resultFactory = [&displayText](const MessageProcessorContext &) {
        return successfulResult(displayText);
    };

    const ProcessorExecutionResult result = engine.execute(
        revision(),
        ProcessorRuntimeConformance::context());

    QVERIFY(result.succeeded());
    QCOMPARE(result.preview, displayText);
}

void MessageProcessorEngineTest::passesCommonRuntimeConformanceHarness()
{
    const auto adapter = QSharedPointer<FakeRuntimeAdapter>::create(
        descriptor(QStringLiteral("qt-qjs"), QStringLiteral("javascript")));
    ProcessorRuntimeConformance::verify(adapter, revision());
    QCOMPARE(adapter->prepareCalls, 1);
    QCOMPARE(adapter->executeCalls, 1);
    QCOMPARE(adapter->lastContext.payload, QByteArray::fromHex("00017f80ff"));
    QCOMPARE(
        adapter->lastContext.parameters.value(QStringLiteral("label")).toString(),
        QStringLiteral("传感器"));
}

QTEST_GUILESS_MAIN(MessageProcessorEngineTest)

#include "test_messageprocessorengine.moc"
