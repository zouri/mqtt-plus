#pragma once

#include "domain/messagerecord.h"
#include "presentation/eventrow.h"

#include <QHash>
#include <QVariantList>
#include <QVector>

namespace EventRenderer {
qint64 firstHistoryId(const QVector<EventRow> &rows);
bool containsLaunchDivider(const QVector<EventRow> &rows);
bool containsRowsBeforeLaunch(
    const QVector<EventRow> &rows,
    const QString &launchTimestamp);
bool startsWithCurrentLaunchRows(
    const QVector<EventRow> &rows,
    const QString &launchTimestamp);
EventRow launchDividerRow(const QString &launchTimestamp);
EventRow eventRow(
    qint64 historyId,
    const QString &timestamp,
    const QString &channel,
    const QString &message);
EventRow renderMessageRow(
    const MessageRecord &row,
    const QHash<QString, int> &subscriptionFormats,
    const QHash<QString, QString> &subscriptionColors = {},
    const QHash<QString, QString> &subscriptionAliases = {},
    const QString &explicitTopicColor = {},
    const QString &explicitAlias = {});
QVector<EventRow> loadHistoryRows(
    const QVector<MessageRecord> &rows,
    const QHash<QString, int> &subscriptionFormats,
    const QHash<QString, QString> &subscriptionColors,
    const QHash<QString, QString> &subscriptionAliases,
    const QString &launchTimestamp,
    bool includeLaunchDivider);
QVector<EventRow> loadLogRows(
    const QVariantList &rows,
    const QString &launchTimestamp,
    bool includeLaunchDivider);
}
