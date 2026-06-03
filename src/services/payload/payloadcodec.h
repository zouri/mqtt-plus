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

    static bool topicFilterMatches(const QString &filter, const QString &topic);
    static PayloadFormat resolveTopicFormat(
        const QHash<QString, int> &topicFormats,
        const QString &topic);
};
