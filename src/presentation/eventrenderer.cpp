#include "eventrenderer.h"

#include "services/apputils.h"
#include "services/payload/payloadcodec.h"

using namespace AppUtils;

namespace {
QString startupDividerLabel()
{
    return QStringLiteral("Current launch");
}

bool isStartupDividerTitle(const QString &title)
{
    return title == startupDividerLabel() || title == QStringLiteral("Current launch");
}

QString resolveTopicValue(const QHash<QString, QString> &values, const QString &topic)
{
    QString value;
    QString bestFilter;
    int bestScore = -1;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.value().isEmpty() || !PayloadCodec::topicFilterMatches(it.key(), topic)) {
            continue;
        }

        const int score = topicSpecificityScore(it.key());
        if (score > bestScore || (score == bestScore && (bestFilter.isEmpty() || it.key() < bestFilter))) {
            bestScore = score;
            bestFilter = it.key();
            value = it.value();
        }
    }
    return value;
}
}

namespace EventRenderer {
qint64 firstHistoryId(const QVariantList &rows)
{
    for (const QVariant &item : rows) {
        const qint64 id = item.toMap().value(QStringLiteral("historyId")).toLongLong();
        if (id > 0) {
            return id;
        }
    }
    return 0;
}

bool containsLaunchDivider(const QVariantList &rows)
{
    for (const QVariant &item : rows) {
        const QVariantMap row = item.toMap();
        if (row.value(QStringLiteral("kind")).toString() == QStringLiteral("divider")
                && isStartupDividerTitle(row.value(QStringLiteral("title")).toString())) {
            return true;
        }
    }
    return false;
}

bool containsRowsBeforeLaunch(const QVariantList &rows, const QString &launchTimestamp)
{
    for (const QVariant &item : rows) {
        const QVariantMap row = item.toMap();
        const QString rowTimestamp = row.value(
            QStringLiteral("timestampRaw"),
            row.value(QStringLiteral("timestamp"))).toString();
        if (row.value(QStringLiteral("kind")).toString() != QStringLiteral("divider")
                && rowTimestamp < launchTimestamp) {
            return true;
        }
    }
    return false;
}

bool startsWithCurrentLaunchRows(const QVariantList &rows, const QString &launchTimestamp)
{
    for (const QVariant &item : rows) {
        const QVariantMap row = item.toMap();
        if (row.value(QStringLiteral("kind")).toString() == QStringLiteral("divider")) {
            continue;
        }
        return row.value(
                   QStringLiteral("timestampRaw"),
                   row.value(QStringLiteral("timestamp"))).toString() >= launchTimestamp;
    }
    return false;
}

QVariantMap launchDividerRow(const QString &launchTimestamp)
{
    QVariantMap dividerRow;
    dividerRow.insert(QStringLiteral("timestamp"), displayTimestamp(launchTimestamp));
    dividerRow.insert(QStringLiteral("timestampRaw"), launchTimestamp);
    dividerRow.insert(QStringLiteral("historyId"), 0);
    dividerRow.insert(QStringLiteral("kind"), QStringLiteral("divider"));
    dividerRow.insert(QStringLiteral("title"), startupDividerLabel());
    dividerRow.insert(QStringLiteral("topic"), QString());
    dividerRow.insert(QStringLiteral("payload"), QString());
    dividerRow.insert(QStringLiteral("payloadFormat"), QString());
    dividerRow.insert(QStringLiteral("payloadSize"), 0);
    dividerRow.insert(QStringLiteral("topicColor"), QString());
    dividerRow.insert(QStringLiteral("direction"), QString());
    dividerRow.insert(QStringLiteral("alias"), QString());
    dividerRow.insert(QStringLiteral("qos"), -1);
    dividerRow.insert(QStringLiteral("retain"), false);
    dividerRow.insert(QStringLiteral("retainKnown"), false);
    dividerRow.insert(QStringLiteral("parsedPayload"), QString());
    dividerRow.insert(QStringLiteral("payloadState"), QString());
    dividerRow.insert(QStringLiteral("payloadHash"), QString());
    return dividerRow;
}

QVariantMap eventRow(qint64 historyId, const QString &timestamp, const QString &channel, const QString &message)
{
    QVariantMap row;
    row.insert(QStringLiteral("historyId"), historyId);
    row.insert(QStringLiteral("timestamp"), displayTimestamp(timestamp));
    row.insert(QStringLiteral("timestampRaw"), timestamp);
    row.insert(QStringLiteral("kind"), QStringLiteral("event"));
    row.insert(QStringLiteral("title"), channel);
    row.insert(QStringLiteral("topic"), channel);
    row.insert(QStringLiteral("payload"), message);
    row.insert(QStringLiteral("payloadFormat"), QStringLiteral("Event"));
    row.insert(QStringLiteral("payloadSize"), 0);
    row.insert(QStringLiteral("topicColor"), QString());
    row.insert(QStringLiteral("direction"), QString());
    row.insert(QStringLiteral("alias"), QString());
    row.insert(QStringLiteral("qos"), -1);
    row.insert(QStringLiteral("retain"), false);
    row.insert(QStringLiteral("retainKnown"), false);
    row.insert(QStringLiteral("parsedPayload"), QString());
    row.insert(QStringLiteral("payloadState"), QString());
    row.insert(QStringLiteral("payloadHash"), QString());
    return row;
}

QVariantMap renderHistoryRow(
    const QVariantMap &row,
    const QHash<QString, int> &subscriptionFormats,
    const QHash<QString, QString> &subscriptionColors,
    const QHash<QString, QString> &subscriptionAliases)
{
    const QString kind = row.value(QStringLiteral("entry_type"), QStringLiteral("message")).toString();
    const QString timestamp = row.value(QStringLiteral("timestamp")).toString();
    const QString topic = row.value(QStringLiteral("topic")).toString();

    QVariantMap rendered;
    rendered.insert(QStringLiteral("historyId"), row.value(QStringLiteral("id")).toLongLong());
    rendered.insert(QStringLiteral("timestamp"), displayTimestamp(timestamp));
    rendered.insert(QStringLiteral("timestampRaw"), timestamp);
    rendered.insert(QStringLiteral("topic"), topic);

    if (kind == QStringLiteral("divider")) {
        rendered.insert(QStringLiteral("kind"), QStringLiteral("divider"));
        rendered.insert(QStringLiteral("title"), row.value(QStringLiteral("payload"), startupDividerLabel()).toString());
        rendered.insert(QStringLiteral("payload"), QString());
        rendered.insert(QStringLiteral("payloadFormat"), QString());
        rendered.insert(QStringLiteral("payloadSize"), 0);
        rendered.insert(QStringLiteral("topicColor"), QString());
        rendered.insert(QStringLiteral("direction"), QString());
        rendered.insert(QStringLiteral("alias"), QString());
        return rendered;
    }

    if (kind == QStringLiteral("event")) {
        rendered.insert(QStringLiteral("kind"), QStringLiteral("event"));
        rendered.insert(QStringLiteral("title"), topic);
        rendered.insert(QStringLiteral("payload"), row.value(QStringLiteral("payload")).toString());
        rendered.insert(QStringLiteral("payloadFormat"), QStringLiteral("Event"));
        rendered.insert(QStringLiteral("payloadSize"), 0);
        rendered.insert(QStringLiteral("topicColor"), QString());
        rendered.insert(QStringLiteral("direction"), QString());
        rendered.insert(QStringLiteral("alias"), QString());
        return rendered;
    }

    const QByteArray payloadBytes = row.value(QStringLiteral("payload_bytes")).toByteArray();

    qint64 payloadSize = row.value(QStringLiteral("payload_size")).toLongLong();
    if (payloadSize <= 0) {
        payloadSize = payloadBytes.size();
    }

    QString payloadState = row.value(QStringLiteral("payload_state"), QStringLiteral("full")).toString();
    if (payloadState.isEmpty()) {
        payloadState = QStringLiteral("full");
    }
    QString payloadPreview = row.value(QStringLiteral("payload_preview")).toString();
    const bool renderingStoredPayload = payloadState != QStringLiteral("skipped")
        && (!payloadBytes.isEmpty() || payloadPreview.isEmpty());
    const bool renderingFullPayloadFromPreview = payloadState == QStringLiteral("full")
        && payloadBytes.isEmpty()
        && !payloadPreview.isEmpty();
    const bool renderedPayloadIsPreviewOnly = renderingFullPayloadFromPreview
        && payloadSize > payloadPreview.toUtf8().size();

    const int storedPayloadFormat = row.value(QStringLiteral("payload_format"), -1).toInt();
    const PayloadFormat format = storedPayloadFormat >= 0
        ? PayloadCodec::formatFromInt(storedPayloadFormat)
        : PayloadCodec::resolveTopicFormat(subscriptionFormats, topic);
    QString parseError;
    QString renderedPayload;
    QString decodedPayload;
    if (renderingStoredPayload) {
        decodedPayload = PayloadCodec::decodeForDisplay(format, payloadBytes, parseError);
        renderedPayload = decodedPayload;
        if (!parseError.isEmpty()) {
            renderedPayload = QStringLiteral("%1\n%2").arg(renderedPayload, payloadPreview);
        }
    } else if (renderingFullPayloadFromPreview) {
        decodedPayload = PayloadCodec::decodeForDisplay(format, payloadPreview.toUtf8(), parseError);
        renderedPayload = decodedPayload;
        if (!parseError.isEmpty()) {
            decodedPayload = payloadPreview;
            renderedPayload = payloadPreview;
        }
    } else {
        renderedPayload = payloadState == QStringLiteral("skipped")
            ? QString()
            : payloadPreview;
    }

    const QString scriptError = row.value(QStringLiteral("parse_error")).toString();
    const QString parsedFormat = row.value(QStringLiteral("parsed_format")).toString();
    const bool hasScriptResult = !parsedFormat.isEmpty() || !scriptError.isEmpty();
    if (!scriptError.isEmpty()) {
        renderedPayload = QStringLiteral("%1\nLua Error: %2").arg(renderedPayload, scriptError);
    } else if (hasScriptResult) {
        renderedPayload = row.value(QStringLiteral("parsed_payload")).toString();
    }

    rendered.insert(QStringLiteral("kind"), QStringLiteral("message"));
    rendered.insert(QStringLiteral("title"), topic);
    rendered.insert(QStringLiteral("payload"), renderedPayload);
    rendered.insert(QStringLiteral("direction"), row.value(QStringLiteral("direction"), QStringLiteral("incoming")).toString());
    const QString explicitAlias = row.value(QStringLiteral("topic_alias")).toString();
    rendered.insert(
        QStringLiteral("alias"),
        explicitAlias.isEmpty() ? resolveTopicValue(subscriptionAliases, topic) : explicitAlias);
    rendered.insert(QStringLiteral("qos"), row.value(QStringLiteral("qos"), -1).toInt());
    rendered.insert(QStringLiteral("retain"), row.value(QStringLiteral("retain")).toBool());
    rendered.insert(QStringLiteral("retainKnown"), row.value(QStringLiteral("retain_known")).toBool());
    rendered.insert(QStringLiteral("parsedPayload"), row.value(QStringLiteral("parsed_payload")).toString());
    rendered.insert(QStringLiteral("payloadState"), payloadState);
    rendered.insert(QStringLiteral("payloadHash"), row.value(QStringLiteral("payload_hash")).toString());
    const QString explicitTopicColor = row.value(QStringLiteral("topic_color")).toString();
    rendered.insert(
        QStringLiteral("topicColor"),
        explicitTopicColor.isEmpty() ? resolveTopicValue(subscriptionColors, topic) : explicitTopicColor);
    rendered.insert(
        QStringLiteral("payloadFormat"),
        !scriptError.isEmpty()
            ? QStringLiteral("Lua Error")
            : (hasScriptResult ? parsedFormat : (renderedPayloadIsPreviewOnly
                    ? QStringLiteral("%1 preview").arg(PayloadCodec::formatName(format))
                    : (renderingStoredPayload
                    ? PayloadCodec::formatName(format)
                    : (payloadState == QStringLiteral("skipped")
                    ? QStringLiteral("Skipped")
                    : (payloadState == QStringLiteral("truncated")
                        ? QStringLiteral("Truncated")
                        : (payloadState == QStringLiteral("raw_only")
                            ? QStringLiteral("%1 · raw").arg(PayloadCodec::formatName(format))
                            : PayloadCodec::formatName(format))))))));
    rendered.insert(QStringLiteral("payloadSize"), payloadSize);
    rendered.insert(
        QStringLiteral("testPayload"),
        renderingStoredPayload
            ? decodedPayload
            : payloadPreview);
    rendered.insert(QStringLiteral("testFormat"), static_cast<int>(format));
    rendered.insert(QStringLiteral("testFormatName"), PayloadCodec::formatName(format));
    return rendered;
}

QVariantList loadHistoryRows(
    const QVariantList &rows,
    const QHash<QString, int> &subscriptionFormats,
    const QHash<QString, QString> &subscriptionColors,
    const QHash<QString, QString> &subscriptionAliases,
    const QString &launchTimestamp,
    bool includeLaunchDivider)
{
    QVariantList previousRows;
    QVariantList currentRows;
    previousRows.reserve(rows.size());
    currentRows.reserve(rows.size());

    for (const QVariant &item : rows) {
        const QVariantMap row = item.toMap();
        if (row.value(QStringLiteral("entry_type")).toString() == QStringLiteral("divider")) {
            continue;
        }

        const QVariantMap renderedRow = renderHistoryRow(row, subscriptionFormats, subscriptionColors, subscriptionAliases);
        if (row.value(QStringLiteral("timestamp")).toString() < launchTimestamp) {
            previousRows.append(renderedRow);
        } else {
            currentRows.append(renderedRow);
        }
    }

    QVariantList rendered;
    rendered.reserve(previousRows.size() + currentRows.size() + (previousRows.isEmpty() ? 0 : 1));
    for (const QVariant &item : previousRows) {
        rendered.append(item);
    }

    if (includeLaunchDivider && !previousRows.isEmpty()) {
        rendered.append(launchDividerRow(launchTimestamp));
    }

    for (const QVariant &item : currentRows) {
        rendered.append(item);
    }
    return rendered;
}
}
