#include "services/scripting/luarunner.h"

#include <QtTest/QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

class LuaRunnerTest : public QObject
{
    Q_OBJECT

private slots:
    void reusesConstantsWithIsolatedLogicState();
    void supportsLegacyParseFunction();
    void supportsLegacyConstantsGlobal();
    void changedCodeReplacesRuntime();
    void runtimeErrorsDoNotLeakState();
    void constantsAreReadOnly();
    void constantsCanBeReturned();
    void ignoresForgedConstantProxyMarker();
    void resultMetatableLookupCannotRun();
    void enforcesConstantDepthLimit();
    void libraryMutationsAreIsolated();
    void rejectsUnsupportedConstants();
    void rejectsCyclicConstants();
    void parsesProjectMessageScript();
    void reportsTruncatedProjectMessageAsFailure();
};

namespace {
LuaScriptContext context(const QString &topic = QStringLiteral("devices/test"))
{
    LuaScriptContext result;
    result.topic = topic;
    result.payloadBytes = QByteArrayLiteral("payload");
    result.decodedPayload = QStringLiteral("payload");
    result.format = PayloadFormat::Plaintext;
    result.timestamp = QStringLiteral("2026-07-31T00:00:00.000Z");
    return result;
}
}

void LuaRunnerTest::reusesConstantsWithIsolatedLogicState()
{
    LuaRunner::RuntimeCache cache;
    const QString script = QStringLiteral(
        "function constants()\n"
        "    return { offset = 40, values = { 1, 2, 3 }, labels = { a = 'A', b = 'B' } }\n"
        "end\n"
        "local calls = 0\n"
        "function parse(ctx, const)\n"
        "    calls = calls + 1\n"
        "    local sum = 0\n"
        "    for _, value in ipairs(const.values) do sum = sum + value end\n"
        "    local labelCount = 0\n"
        "    for _ in pairs(const.labels) do labelCount = labelCount + 1 end\n"
        "    return const.offset + calls + sum + labelCount + #const.values\n"
        "end\n");

    const LuaScriptResult first = cache.run(QStringLiteral("isolated"), script, context());
    const LuaScriptResult second = cache.run(QStringLiteral("isolated"), script, context());

    QVERIFY2(first.success, qPrintable(first.error));
    QVERIFY2(second.success, qPrintable(second.error));
    QCOMPARE(first.output, QStringLiteral("52"));
    QCOMPARE(second.output, QStringLiteral("52"));
}

void LuaRunnerTest::supportsLegacyParseFunction()
{
    LuaRunner::RuntimeCache cache;
    const QString script = QStringLiteral(
        "function parse(ctx)\n"
        "    return ctx.topic\n"
        "end\n");

    const LuaScriptResult result = cache.run(QStringLiteral("legacy"), script, context());

    QVERIFY2(result.success, qPrintable(result.error));
    QCOMPARE(result.output, QStringLiteral("devices/test"));
}

void LuaRunnerTest::supportsLegacyConstantsGlobal()
{
    LuaRunner::RuntimeCache cache;
    const QString script = QStringLiteral(
        "constants = { value = 'legacy' }\n"
        "function parse(ctx) return constants.value end\n");

    const LuaScriptResult result = cache.run(QStringLiteral("legacy-constants"), script, context());

    QVERIFY2(result.success, qPrintable(result.error));
    QCOMPARE(result.output, QStringLiteral("legacy"));
}

void LuaRunnerTest::changedCodeReplacesRuntime()
{
    LuaRunner::RuntimeCache cache;
    const QString firstCode = QStringLiteral(
        "function constants() return { value = 'first' } end\n"
        "function parse(ctx, const) return const.value end\n");
    const QString secondCode = QStringLiteral(
        "function constants() return { value = 'second' } end\n"
        "function parse(ctx, const) return const.value end\n");

    const LuaScriptResult first = cache.run(QStringLiteral("changed"), firstCode, context());
    const LuaScriptResult changed = cache.run(QStringLiteral("changed"), secondCode, context());

    QVERIFY2(first.success, qPrintable(first.error));
    QVERIFY2(changed.success, qPrintable(changed.error));
    QCOMPARE(first.output, QStringLiteral("first"));
    QCOMPARE(changed.output, QStringLiteral("second"));
}

void LuaRunnerTest::runtimeErrorsDoNotLeakState()
{
    LuaRunner::RuntimeCache cache;
    const QString script = QStringLiteral(
        "local calls = 0\n"
        "function parse(ctx)\n"
        "    calls = calls + 1\n"
        "    if ctx.topic == 'bad' then error('bad payload') end\n"
        "    return calls\n"
        "end\n");

    const LuaScriptResult first = cache.run(QStringLiteral("error"), script, context());
    const LuaScriptResult failed = cache.run(QStringLiteral("error"), script, context(QStringLiteral("bad")));
    const LuaScriptResult recovered = cache.run(QStringLiteral("error"), script, context());

    QVERIFY2(first.success, qPrintable(first.error));
    QVERIFY(!failed.success);
    QVERIFY(failed.error.contains(QStringLiteral("bad payload")));
    QVERIFY2(recovered.success, qPrintable(recovered.error));
    QCOMPARE(first.output, QStringLiteral("1"));
    QCOMPARE(recovered.output, QStringLiteral("1"));
}

void LuaRunnerTest::constantsAreReadOnly()
{
    LuaRunner::RuntimeCache cache;
    const QString script = QStringLiteral(
        "function constants() return { nested = { value = 1 } } end\n"
        "function parse(ctx, const)\n"
        "    if ctx.topic == 'mutate' then const.nested.value = 2 end\n"
        "    return const.nested.value\n"
        "end\n");

    const LuaScriptResult failed = cache.run(
        QStringLiteral("readonly"),
        script,
        context(QStringLiteral("mutate")));
    const LuaScriptResult recovered = cache.run(QStringLiteral("readonly"), script, context());

    QVERIFY(!failed.success);
    QVERIFY(failed.error.contains(QStringLiteral("read-only")));
    QVERIFY2(recovered.success, qPrintable(recovered.error));
    QCOMPARE(recovered.output, QStringLiteral("1"));
}

void LuaRunnerTest::constantsCanBeReturned()
{
    LuaRunner::RuntimeCache cache;
    const QString script = QStringLiteral(
        "function constants() return { name = 'cached', values = { 1, 2, 3 } } end\n"
        "function parse(ctx, const) return const end\n");

    const LuaScriptResult result = cache.run(QStringLiteral("return-constants"), script, context());

    QVERIFY2(result.success, qPrintable(result.error));
    const QJsonDocument document = QJsonDocument::fromJson(result.output.toUtf8());
    QVERIFY(document.isObject());
    QCOMPARE(document.object().value(QStringLiteral("name")).toString(), QStringLiteral("cached"));
    QCOMPARE(document.object().value(QStringLiteral("values")).toArray().size(), 3);
}

void LuaRunnerTest::ignoresForgedConstantProxyMarker()
{
    LuaRunner::RuntimeCache cache;
    const QString script = QStringLiteral(
        "function parse(ctx)\n"
        "    local result = {}\n"
        "    setmetatable(result, { __mqtt_plus_constant_proxy = true, __index = result })\n"
        "    return result\n"
        "end\n");

    const LuaScriptResult result = cache.run(QStringLiteral("forged-proxy"), script, context());

    QVERIFY2(result.success, qPrintable(result.error));
    const QJsonDocument document = QJsonDocument::fromJson(result.output.toUtf8());
    QVERIFY(document.isArray());
    QVERIFY(document.array().isEmpty());
}

void LuaRunnerTest::resultMetatableLookupCannotRun()
{
    LuaRunner::RuntimeCache cache;
    const QString script = QStringLiteral(
        "function parse(ctx)\n"
        "    local result = {}\n"
        "    local metadata = {}\n"
        "    setmetatable(metadata, { __index = function() error('metadata lookup') end })\n"
        "    setmetatable(result, metadata)\n"
        "    return result\n"
        "end\n");

    const LuaScriptResult result = cache.run(QStringLiteral("metatable-lookup"), script, context());

    QVERIFY2(result.success, qPrintable(result.error));
    const QJsonDocument document = QJsonDocument::fromJson(result.output.toUtf8());
    QVERIFY(document.isArray());
    QVERIFY(document.array().isEmpty());
}

void LuaRunnerTest::enforcesConstantDepthLimit()
{
    LuaRunner::RuntimeCache cache;
    const auto scriptForDepth = [](int depth) {
        return QStringLiteral(
            "function constants()\n"
            "    local root = {}\n"
            "    local current = root\n"
            "    for index = 1, %1 do\n"
            "        current.child = {}\n"
            "        current = current.child\n"
            "    end\n"
            "    return root\n"
            "end\n"
            "function parse(ctx, const) return 'ok' end\n")
            .arg(depth);
    };

    const LuaScriptResult maximum = cache.run(
        QStringLiteral("maximum-depth"),
        scriptForDepth(32),
        context());
    const LuaScriptResult tooDeep = cache.run(
        QStringLiteral("excessive-depth"),
        scriptForDepth(33),
        context());

    QVERIFY2(maximum.success, qPrintable(maximum.error));
    QVERIFY(!tooDeep.success);
    QVERIFY(tooDeep.error.contains(QStringLiteral("nesting is too deep")));
}

void LuaRunnerTest::libraryMutationsAreIsolated()
{
    LuaRunner::RuntimeCache cache;
    const QString script = QStringLiteral(
        "function parse(ctx)\n"
        "    if ctx.topic == 'mutate' then math.marker = 1 return math.marker end\n"
        "    return math.marker == nil and 'clean' or 'dirty'\n"
        "end\n");

    const LuaScriptResult mutated = cache.run(
        QStringLiteral("libraries"),
        script,
        context(QStringLiteral("mutate")));
    const LuaScriptResult isolated = cache.run(QStringLiteral("libraries"), script, context());

    QVERIFY2(mutated.success, qPrintable(mutated.error));
    QVERIFY2(isolated.success, qPrintable(isolated.error));
    QCOMPARE(mutated.output, QStringLiteral("1"));
    QCOMPARE(isolated.output, QStringLiteral("clean"));
}

void LuaRunnerTest::rejectsUnsupportedConstants()
{
    LuaRunner::RuntimeCache cache;
    const QString script = QStringLiteral(
        "function constants() return { callback = function() end } end\n"
        "function parse(ctx, const) return ctx.decoded end\n");

    const LuaScriptResult result = cache.run(QStringLiteral("unsupported"), script, context());

    QVERIFY(!result.success);
    QVERIFY(result.error.contains(QStringLiteral("may only contain")));
}

void LuaRunnerTest::rejectsCyclicConstants()
{
    LuaRunner::RuntimeCache cache;
    const QString script = QStringLiteral(
        "function constants()\n"
        "    local value = {}\n"
        "    value.self = value\n"
        "    return value\n"
        "end\n"
        "function parse(ctx, const) return ctx.decoded end\n");

    const LuaScriptResult result = cache.run(QStringLiteral("cyclic"), script, context());

    QVERIFY(!result.success);
    QVERIFY(result.error.contains(QStringLiteral("must not contain cycles")));
}

void LuaRunnerTest::parsesProjectMessageScript()
{
    QFile file(QStringLiteral(MQTT_PLUS_SOURCE_DIR "/ParseMessage.lua"));
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(file.errorString()));

    LuaRunner::RuntimeCache cache;
    LuaScriptContext scriptContext = context();
    scriptContext.payloadBytes = QByteArray::fromHex(
        QByteArrayLiteral("00000000140000000000000000000000000000000b00080007010203"));
    const QString code = QString::fromUtf8(file.readAll());
    const QString expected = QStringLiteral(
        "消息类型: 接收心跳 | 消息ID: 11 | 头长度: 20\n"
        "消息ID: 11 | 数据长度: 8 | 发送者状态: 7\n"
        "其他信息: [1,2,3] | -: - | -: -");

    const LuaScriptResult first = cache.run(QStringLiteral("parse-message"), code, scriptContext);
    const LuaScriptResult second = cache.run(QStringLiteral("parse-message"), code, scriptContext);

    QVERIFY2(first.success, qPrintable(first.error));
    QVERIFY2(second.success, qPrintable(second.error));
    QCOMPARE(first.output, expected);
    QCOMPARE(second.output, expected);
}

void LuaRunnerTest::reportsTruncatedProjectMessageAsFailure()
{
    QFile file(QStringLiteral(MQTT_PLUS_SOURCE_DIR "/ParseMessage.lua"));
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(file.errorString()));

    LuaRunner::RuntimeCache cache;
    LuaScriptContext scriptContext = context();
    scriptContext.payloadBytes = QByteArray::fromHex(
        QByteArrayLiteral("00000000140000000000000000000000000000000b000800"));
    const QString code = QString::fromUtf8(file.readAll());

    const LuaScriptResult result = cache.run(
        QStringLiteral("parse-message-truncated"),
        code,
        scriptContext);

    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(result.output.contains(QStringLiteral("结果: 解析失败")));
    QVERIFY(result.output.contains(QStringLiteral("消息体长度")));
}

QTEST_APPLESS_MAIN(LuaRunnerTest)

#include "test_luarunner.moc"
