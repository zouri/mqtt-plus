#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

enum class PayloadFormat {
    Plaintext = 0,
    Json = 1,
    Base64 = 2,
    Hex = 3,
    Cbor = 4,
    MsgPack = 5
};

class PayloadCodec
{
public:
    static QStringList formatNames();
    static PayloadFormat formatFromInt(int value);
    static PayloadFormat formatFromId(const QString &id, bool *ok = nullptr);
    static QString formatId(PayloadFormat format);
    static QString formatName(PayloadFormat format);

    static bool encodeForPublish(
        PayloadFormat format,
        const QString &input,
        QByteArray &output,
        QString &error);
    static QString decodeForDisplay(
        PayloadFormat format,
        const QByteArray &payloadBytes,
        QString &error);

    static PayloadFormat resolveTopicFormat(
        const QHash<QString, int> &topicFormats,
        const QString &topic);
};
