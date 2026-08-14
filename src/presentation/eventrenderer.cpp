#include "eventrenderer.h"

#include "services/apputils.h"
#include "services/payload/payloadcodec.h"

using namespace AppUtils;

namespace {
constexpr qsizetype kListTextCharacterLimit = 64 * 1024;
constexpr qsizetype kInlineExpandedLineLimit = 4096;

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

        const int score = PayloadCodec::topicSpecificityScore(it.key());
        if (score > bestScore
            || (score == bestScore && (bestFilter.isEmpty() || it.key() < bestFilter))) {
            bestScore = score;
            bestFilter = it.key();
            value = it.value();
        }
    }
    return value;
}

QString boundedListText(const QString &text)
{
    return text.left(kListTextCharacterLimit);
}

bool exceedsInlineLineLimit(const QString &text)
{
    return text.count(QLatin1Char('\n')) >= kInlineExpandedLineLimit;
}
} // namespace

namespace EventRenderer {
qint64 firstHistoryId(const QVector<EventRow> &rows)
{
    for (const EventRow &row : rows) {
        if (row.historyId > 0) {
            return row.historyId;
        }
    }
    return 0;
}

bool containsLaunchDivider(const QVector<EventRow> &rows)
{
    for (const EventRow &row : rows) {
        if (row.kind == QStringLiteral("divider")
            && isStartupDividerTitle(row.title)) {
            return true;
        }
    }
    return false;
}

bool containsRowsBeforeLaunch(
    const QVector<EventRow> &rows,
    const QString &launchTimestamp)
{
    for (const EventRow &row : rows) {
        const QString timestamp = row.timestampRaw.isEmpty()
            ? row.timestamp
            : row.timestampRaw;
        if (row.kind != QStringLiteral("divider") && timestamp < launchTimestamp) {
            return true;
        }
    }
    return false;
}

bool startsWithCurrentLaunchRows(
    const QVector<EventRow> &rows,
    const QString &launchTimestamp)
{
    for (const EventRow &row : rows) {
        if (row.kind == QStringLiteral("divider")) {
            continue;
        }
        const QString timestamp = row.timestampRaw.isEmpty()
            ? row.timestamp
            : row.timestampRaw;
        return timestamp >= launchTimestamp;
    }
    return false;
}

EventRow launchDividerRow(const QString &launchTimestamp)
{
    EventRow row;
    row.timestamp = displayTimestamp(launchTimestamp);
    row.timestampRaw = launchTimestamp;
    row.kind = QStringLiteral("divider");
    row.title = startupDividerLabel();
    row.expandedPayloadState.clear();
    return row;
}

EventRow eventRow(
    qint64 historyId,
    const QString &timestamp,
    const QString &channel,
    const QString &message)
{
    EventRow row;
    row.historyId = historyId;
    row.timestamp = displayTimestamp(timestamp);
    row.timestampRaw = timestamp;
    row.kind = QStringLiteral("event");
    row.title = channel;
    row.topic = channel;
    row.payload = message;
    row.payloadFormat = QStringLiteral("Event");
    row.expandedPayloadState.clear();
    return row;
}

EventRow renderMessageRow(
    const MessageRecord &row,
    const QHash<QString, int> &subscriptionFormats,
    const QHash<QString, QString> &subscriptionColors,
    const QHash<QString, QString> &subscriptionAliases,
    const QString &explicitTopicColor,
    const QString &explicitAlias)
{
    EventRow rendered;
    rendered.historyId = row.id;
    rendered.timestamp = displayTimestamp(row.timestamp);
    rendered.timestampRaw = row.timestamp;
    rendered.topic = row.topic;
    rendered.kind = QStringLiteral("message");
    rendered.title = row.topic;

    qint64 payloadSize = row.payloadSize;
    if (payloadSize <= 0) {
        payloadSize = row.payloadBytes.size();
    }

    QString payloadState = row.payloadState;
    if (payloadState.isEmpty()) {
        payloadState = QStringLiteral("full");
    }
    const bool renderingFullPayload = payloadState == QStringLiteral("full");
    const PayloadFormat format = row.payloadFormat >= 0
        ? PayloadCodec::formatFromInt(row.payloadFormat)
        : PayloadCodec::resolveTopicFormat(subscriptionFormats, row.topic);
    const QString parsedPayload = boundedListText(row.displayPayload);
    QString parseState = row.displayState;
    if (parseState.isEmpty()) {
        parseState = !row.displayError.isEmpty()
            ? QStringLiteral("failed")
            : (!row.displayFormat.isEmpty() || !parsedPayload.isEmpty()
                ? QStringLiteral("succeeded")
                : QStringLiteral("not_required"));
    }

    QString decodeError;
    QString renderedPayload;
    QString decodedPayload;
    bool renderedPayloadIsPreviewOnly = false;
    const bool parseHandledInWorker = parseState != QStringLiteral("not_required");
    if (renderingFullPayload && !parseHandledInWorker) {
        const QByteArray displayBytes = !row.payloadPreview.isEmpty() || row.payloadBytes.isEmpty()
            ? row.payloadPreview.toUtf8()
            : row.payloadBytes;
        renderedPayloadIsPreviewOnly = payloadSize > displayBytes.size();
        decodedPayload = PayloadCodec::decodeForDisplay(format, displayBytes, decodeError);
        renderedPayload = decodedPayload;
        if (!decodeError.isEmpty()) {
            renderedPayload = QStringLiteral("%1\n%2").arg(renderedPayload, row.payloadPreview);
        }
    } else {
        renderedPayload = payloadState == QStringLiteral("skipped")
            ? QString()
            : row.payloadPreview;
        decodedPayload = renderedPayload;
        renderedPayloadIsPreviewOnly = payloadSize > row.payloadPreview.toUtf8().size();
    }

    if (parseState == QStringLiteral("failed") && !row.displayError.isEmpty()) {
        const QString errorLabel = row.processorId.isEmpty()
            ? QStringLiteral("Parser Error")
            : QStringLiteral("Processor Error");
        renderedPayload = renderedPayload.isEmpty()
            ? QStringLiteral("%1: %2").arg(errorLabel, row.displayError)
            : QStringLiteral("%1\n%2: %3").arg(renderedPayload, errorLabel, row.displayError);
    } else if (parseState == QStringLiteral("succeeded")) {
        renderedPayload = parsedPayload;
    }
    renderedPayload = boundedListText(renderedPayload);
    decodedPayload = boundedListText(decodedPayload);

    rendered.payload = renderedPayload;
    rendered.direction = messageDirectionName(row.direction);
    rendered.alias = explicitAlias.isEmpty()
        ? resolveTopicValue(subscriptionAliases, row.topic)
        : explicitAlias;
    rendered.qos = row.qos;
    rendered.retain = row.retain;
    rendered.retainKnown = row.retainKnown;
    rendered.parsedPayload = parsedPayload;
    rendered.parseState = parseState;
    rendered.payloadState = payloadState;
    rendered.payloadHash = row.payloadHash;
    rendered.expandedPayloadNeeded = parseState == QStringLiteral("succeeded")
        ? row.displayPayload.size() > parsedPayload.size()
            || exceedsInlineLineLimit(row.displayPayload)
        : parseState == QStringLiteral("not_required")
            && ((payloadState == QStringLiteral("raw_only") && payloadSize > 64)
                || payloadState == QStringLiteral("truncated")
                || exceedsInlineLineLimit(renderedPayload));
    rendered.topicColor = explicitTopicColor.isEmpty()
        ? resolveTopicValue(subscriptionColors, row.topic)
        : explicitTopicColor;

    if (parseState == QStringLiteral("pending")) {
        rendered.payloadFormat = QStringLiteral("Parsing");
    } else if (parseState == QStringLiteral("skipped_overload")) {
        rendered.payloadFormat = QStringLiteral("Parse skipped");
    } else if (parseState == QStringLiteral("failed")) {
        rendered.payloadFormat = row.processorId.isEmpty()
            ? (row.displayFormat.isEmpty()
                ? QStringLiteral("Parser Error")
                : QStringLiteral("%1 Error").arg(row.displayFormat))
            : QStringLiteral("Processor Error");
    } else if (parseState == QStringLiteral("succeeded")) {
        rendered.payloadFormat = row.displayFormat;
    } else if (payloadState == QStringLiteral("skipped")) {
        rendered.payloadFormat = QStringLiteral("Skipped");
    } else if (payloadState == QStringLiteral("truncated")) {
        rendered.payloadFormat = QStringLiteral("Truncated");
    } else if (payloadState == QStringLiteral("raw_only")) {
        rendered.payloadFormat = QStringLiteral("%1 · raw").arg(PayloadCodec::formatName(format));
    } else if (renderedPayloadIsPreviewOnly) {
        rendered.payloadFormat = QStringLiteral("%1 preview").arg(PayloadCodec::formatName(format));
    } else {
        rendered.payloadFormat = PayloadCodec::formatName(format);
    }
    rendered.payloadSize = payloadSize;
    rendered.testPayload = renderingFullPayload
        ? decodedPayload
        : boundedListText(row.payloadPreview);
    rendered.testFormat = static_cast<int>(format);
    rendered.testFormatName = PayloadCodec::formatName(format);
    return rendered;
}

QVector<EventRow> loadHistoryRows(
    const QVector<MessageRecord> &rows,
    const QHash<QString, int> &subscriptionFormats,
    const QHash<QString, QString> &subscriptionColors,
    const QHash<QString, QString> &subscriptionAliases,
    const QString &launchTimestamp,
    bool includeLaunchDivider)
{
    QVector<EventRow> previousRows;
    QVector<EventRow> currentRows;
    previousRows.reserve(rows.size());
    currentRows.reserve(rows.size());

    for (const MessageRecord &row : rows) {
        EventRow rendered = renderMessageRow(
            row,
            subscriptionFormats,
            subscriptionColors,
            subscriptionAliases);
        (row.timestamp < launchTimestamp ? previousRows : currentRows)
            .append(std::move(rendered));
    }

    QVector<EventRow> rendered;
    rendered.reserve(previousRows.size() + currentRows.size() + (previousRows.isEmpty() ? 0 : 1));
    rendered.append(previousRows);
    if (includeLaunchDivider && !previousRows.isEmpty()) {
        rendered.append(launchDividerRow(launchTimestamp));
    }
    rendered.append(currentRows);
    return rendered;
}

QVector<EventRow> loadLogRows(
    const QVariantList &rows,
    const QString &launchTimestamp,
    bool includeLaunchDivider)
{
    QVector<EventRow> previousRows;
    QVector<EventRow> currentRows;
    previousRows.reserve(rows.size());
    currentRows.reserve(rows.size());

    for (const QVariant &item : rows) {
        const QVariantMap stored = item.toMap();
        if (stored.value(QStringLiteral("entry_type")).toString() == QStringLiteral("divider")) {
            continue;
        }
        const QString timestamp = stored.value(QStringLiteral("timestamp")).toString();
        EventRow rendered = eventRow(
            stored.value(QStringLiteral("id")).toLongLong(),
            timestamp,
            stored.value(QStringLiteral("topic")).toString(),
            stored.value(QStringLiteral("payload")).toString());
        (timestamp < launchTimestamp ? previousRows : currentRows)
            .append(std::move(rendered));
    }

    QVector<EventRow> rendered;
    rendered.reserve(previousRows.size() + currentRows.size() + (previousRows.isEmpty() ? 0 : 1));
    rendered.append(previousRows);
    if (includeLaunchDivider && !previousRows.isEmpty()) {
        rendered.append(launchDividerRow(launchTimestamp));
    }
    rendered.append(currentRows);
    return rendered;
}
} // namespace EventRenderer
