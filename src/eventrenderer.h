#pragma once

#include <QHash>
#include <QVariantList>
#include <QVariantMap>

namespace EventRenderer {
qint64 firstHistoryId(const QVariantList &rows);
bool containsLaunchDivider(const QVariantList &rows);
bool containsRowsBeforeLaunch(const QVariantList &rows, const QString &launchTimestamp);
bool startsWithCurrentLaunchRows(const QVariantList &rows, const QString &launchTimestamp);
QVariantMap launchDividerRow(const QString &launchTimestamp);
QVariantMap eventRow(qint64 historyId, const QString &timestamp, const QString &channel, const QString &message);
QVariantMap renderHistoryRow(const QVariantMap &row, const QHash<QString, int> &subscriptionFormats);
QVariantList loadHistoryRows(
    const QVariantList &rows,
    const QHash<QString, int> &subscriptionFormats,
    const QString &launchTimestamp,
    bool includeLaunchDivider);
}
