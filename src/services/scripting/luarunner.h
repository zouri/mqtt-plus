#pragma once

#include <QByteArray>
#include <QString>

#include <memory>

#include "services/payload/payloadcodec.h"

struct LuaScriptContext {
    QString topic;
    QByteArray payloadBytes;
    QString decodedPayload;
    QString decodeError;
    PayloadFormat format = PayloadFormat::Plaintext;
    QString timestamp;
};

struct LuaScriptResult {
    bool success = false;
    QString output;
    QString error;
};

namespace LuaRunner {

// Caches compiled script bytecode and immutable constants while creating a
// fresh Lua environment and function closure for every parse invocation.
class RuntimeCache
{
public:
    RuntimeCache();
    ~RuntimeCache();

    RuntimeCache(const RuntimeCache &) = delete;
    RuntimeCache &operator=(const RuntimeCache &) = delete;

    LuaScriptResult run(
        const QString &scriptId,
        const QString &code,
        const LuaScriptContext &context);
    void clear();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace LuaRunner
