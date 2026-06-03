#pragma once

#include <QByteArray>
#include <QString>

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

class LuaRunner
{
public:
    static LuaScriptResult run(const QString &code, const LuaScriptContext &context);
};
