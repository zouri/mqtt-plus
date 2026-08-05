#pragma once

#include "services/processors/messageprocessorengine.h"
#include "services/processors/processorruntimeadapter.h"
#include "services/processors/processorvaluecodec.h"

#include <QtTest/QtTest>

namespace ProcessorRuntimeConformance {

inline MessageProcessorContext context()
{
    MessageProcessorContext context;
    context.topic = QStringLiteral("devices/温度");
    context.payload = QByteArray::fromHex("00017f80ff");
    context.receivedAt = QStringLiteral("2026-08-05T08:00:00.123Z");
    context.format = QStringLiteral("hex");
    context.decoded = QStringLiteral("decoded text");
    context.decodeError = QStringLiteral("sample decode warning");
    context.parameters.insert(QStringLiteral("scale"), 10);
    context.parameters.insert(QStringLiteral("label"), QStringLiteral("传感器"));
    return context;
}

inline QCborValue expectedValue(const MessageProcessorContext &context)
{
    QCborMap contextValue;
    contextValue.insert(QStringLiteral("topic"), context.topic);
    contextValue.insert(QStringLiteral("payload"), context.payload);
    contextValue.insert(QStringLiteral("receivedAt"), context.receivedAt);
    contextValue.insert(QStringLiteral("format"), context.format);
    contextValue.insert(QStringLiteral("decoded"), context.decoded);
    contextValue.insert(QStringLiteral("decodeError"), context.decodeError);
    contextValue.insert(QStringLiteral("parameters"), context.parameters);

    QCborArray array;
    array.append(nullptr);
    array.append(true);
    array.append(42);
    array.append(3.5);
    array.append(QStringLiteral("value"));
    array.append(QByteArrayLiteral("abc"));

    QCborMap result;
    result.insert(QStringLiteral("context"), contextValue);
    result.insert(QStringLiteral("values"), array);
    return result;
}

inline void verify(
    const QSharedPointer<ProcessorRuntimeAdapter> &adapter,
    const ProcessorRevisionSnapshot &revision)
{
    MessageProcessorEngine engine({adapter});
    const ProcessorValidationResult validation = engine.validate(revision);
    QCOMPARE(
        static_cast<int>(validation.state),
        static_cast<int>(ProcessorValidationState::Ready));

    const MessageProcessorContext input = context();
    const ProcessorExecutionResult result = engine.execute(revision, input);
    QCOMPARE(
        static_cast<int>(result.state),
        static_cast<int>(ProcessorExecutionState::Succeeded));
    QCOMPARE(
        ProcessorValueCodec::encodeCanonical(result.value),
        ProcessorValueCodec::encodeCanonical(expectedValue(input)));
    QVERIFY(!result.preview.isEmpty());
    QVERIFY(result.durationMicroseconds >= 0);
}

} // namespace ProcessorRuntimeConformance
