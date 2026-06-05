#include "eventrenderer.h"

#include "services/payload/payloadcodec.h"

#include <QCoreApplication>

namespace {
QString startupDividerLabel()
{
    return QCoreApplication::translate("EventRenderer", "Current launch");
}

bool isStartupDividerTitle(const QString &title)
{
    return title == startupDividerLabel() || title == QStringLiteral("Current launch");
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
        if (row.value(QStringLiteral("kind")).toString() != QStringLiteral("divider")
                && row.value(QStringLiteral("timestamp")).toString() < launchTimestamp) {
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
        return row.value(QStringLiteral("timestamp")).toString() >= launchTimestamp;
    }
    return false;
}

QVariantMap launchDividerRow(const QString &launchTimestamp)
{
    QVariantMap dividerRow;
    dividerRow.insert(QStringLiteral("timestamp"), launchTimestamp);
    dividerRow.insert(QStringLiteral("historyId"), 0);
    dividerRow.insert(QStringLiteral("kind"), QStringLiteral("divider"));
    dividerRow.insert(QStringLiteral("title"), startupDividerLabel());
    dividerRow.insert(QStringLiteral("topic"), QString());
    dividerRow.insert(QStringLiteral("payload"), QString());
    dividerRow.insert(QStringLiteral("payloadFormat"), QString());
    dividerRow.insert(QStringLiteral("payloadSize"), 0);
    return dividerRow;
}

QVariantMap eventRow(qint64 historyId, const QString &timestamp, const QString &channel, const QString &message)
{
    QVariantMap row;
    row.insert(QStringLiteral("historyId"), historyId);
    row.insert(QStringLiteral("timestamp"), timestamp);
    row.insert(QStringLiteral("kind"), QStringLiteral("event"));
    row.insert(QStringLiteral("title"), channel);
    row.insert(QStringLiteral("topic"), channel);
    row.insert(QStringLiteral("payload"), message);
    row.insert(QStringLiteral("payloadFormat"), QCoreApplication::translate("EventRenderer", "Event"));
    row.insert(QStringLiteral("payloadSize"), 0);
    return row;
}

QVariantMap renderHistoryRow(const QVariantMap &row, const QHash<QString, int> &subscriptionFormats)
{
    const QString kind = row.value(QStringLiteral("entry_type"), QStringLiteral("message")).toString();
    const QString timestamp = row.value(QStringLiteral("timestamp")).toString();
    const QString topic = row.value(QStringLiteral("topic")).toString();

    QVariantMap rendered;
    rendered.insert(QStringLiteral("historyId"), row.value(QStringLiteral("id")).toLongLong());
    rendered.insert(QStringLiteral("timestamp"), timestamp);
    rendered.insert(QStringLiteral("topic"), topic);

    if (kind == QStringLiteral("divider")) {
        rendered.insert(QStringLiteral("kind"), QStringLiteral("divider"));
        rendered.insert(QStringLiteral("title"), row.value(QStringLiteral("payload"), startupDividerLabel()).toString());
        rendered.insert(QStringLiteral("payload"), QString());
        rendered.insert(QStringLiteral("payloadFormat"), QString());
        rendered.insert(QStringLiteral("payloadSize"), 0);
        return rendered;
    }

    if (kind == QStringLiteral("event")) {
        rendered.insert(QStringLiteral("kind"), QStringLiteral("event"));
        rendered.insert(QStringLiteral("title"), topic);
        rendered.insert(QStringLiteral("payload"), row.value(QStringLiteral("payload")).toString());
        rendered.insert(QStringLiteral("payloadFormat"), QCoreApplication::translate("EventRenderer", "Event"));
        rendered.insert(QStringLiteral("payloadSize"), 0);
        return rendered;
    }

    QByteArray payloadBytes =
        QByteArray::fromBase64(row.value(QStringLiteral("payload_b64")).toString().toLatin1());

    const PayloadFormat format = PayloadCodec::resolveTopicFormat(subscriptionFormats, topic);
    QString parseError;
    QString renderedPayload = PayloadCodec::decodeForDisplay(format, payloadBytes, parseError);
    if (!parseError.isEmpty()) {
        renderedPayload = QCoreApplication::translate("EventRenderer", "%1\nRaw(Base64): %2")
                              .arg(renderedPayload, QString::fromLatin1(payloadBytes.toBase64()));
    }

    const QString scriptError = row.value(QStringLiteral("parse_error")).toString();
    const QString parsedFormat = row.value(QStringLiteral("parsed_format")).toString();
    const bool hasScriptResult = !parsedFormat.isEmpty() || !scriptError.isEmpty();
    if (!scriptError.isEmpty()) {
        renderedPayload = QCoreApplication::translate("EventRenderer", "%1\nLua Error: %2")
                              .arg(renderedPayload, scriptError);
    } else if (hasScriptResult) {
        renderedPayload = row.value(QStringLiteral("parsed_payload")).toString();
    }

    rendered.insert(QStringLiteral("kind"), QStringLiteral("message"));
    rendered.insert(QStringLiteral("title"), topic);
    rendered.insert(QStringLiteral("payload"), renderedPayload);
    rendered.insert(
        QStringLiteral("payloadFormat"),
        !scriptError.isEmpty()
            ? QCoreApplication::translate("EventRenderer", "Lua Error")
            : (hasScriptResult ? parsedFormat : PayloadCodec::formatName(format)));
    rendered.insert(QStringLiteral("payloadSize"), payloadBytes.size());
    rendered.insert(QStringLiteral("testPayload"), PayloadCodec::decodeForDisplay(format, payloadBytes, parseError));
    rendered.insert(QStringLiteral("testFormat"), static_cast<int>(format));
    rendered.insert(QStringLiteral("testFormatName"), PayloadCodec::formatName(format));
    return rendered;
}

QVariantList loadHistoryRows(
    const QVariantList &rows,
    const QHash<QString, int> &subscriptionFormats,
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

        const QVariantMap renderedRow = renderHistoryRow(row, subscriptionFormats);
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
