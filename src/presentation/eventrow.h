#pragma once

#include <QMetaType>
#include <QString>
#include <QVariant>
#include <QVariantMap>

struct EventRow {
    QString kind;
    QString timestamp;
    QString timestampRaw;
    QString title;
    QString payload;
    QString payloadFormat;
    qint64 payloadSize = 0;
    QString topic;
    QString topicColor;
    QString testPayload;
    int testFormat = 0;
    QString testFormatName;
    qint64 historyId = 0;
    QString direction;
    QString alias;
    int qos = -1;
    bool retain = false;
    bool retainKnown = false;
    QString parsedPayload;
    QString parseState;
    QString payloadState;
    QString payloadHash;
    QString expandedPayload;
    QString expandedPayloadState = QStringLiteral("idle");
    bool expandedPayloadNeeded = false;

    bool operator==(const EventRow &other) const = default;
};

Q_DECLARE_METATYPE(EventRow)

inline QVariantMap eventRowToVariantMap(const EventRow &row)
{
    return {
        {QStringLiteral("kind"), row.kind},
        {QStringLiteral("timestamp"), row.timestamp},
        {QStringLiteral("title"), row.title},
        {QStringLiteral("payload"), row.payload},
        {QStringLiteral("payloadFormat"), row.payloadFormat},
        {QStringLiteral("payloadSize"), row.payloadSize},
        {QStringLiteral("topic"), row.topic},
        {QStringLiteral("topicColor"), row.topicColor},
        {QStringLiteral("testPayload"), row.testPayload},
        {QStringLiteral("testFormat"), row.testFormat},
        {QStringLiteral("testFormatName"), row.testFormatName},
        {QStringLiteral("historyId"), QString::number(row.historyId)},
        {QStringLiteral("direction"), row.direction},
        {QStringLiteral("alias"), row.alias},
        {QStringLiteral("qos"), row.qos},
        {QStringLiteral("retain"), row.retain},
        {QStringLiteral("retainKnown"), row.retainKnown},
        {QStringLiteral("parsedPayload"), row.parsedPayload},
        {QStringLiteral("parseState"), row.parseState},
        {QStringLiteral("payloadState"), row.payloadState},
        {QStringLiteral("payloadHash"), row.payloadHash},
        {QStringLiteral("expandedPayload"), row.expandedPayload},
        {QStringLiteral("expandedPayloadState"), row.expandedPayloadState},
        {QStringLiteral("expandedPayloadNeeded"), row.expandedPayloadNeeded},
    };
}
