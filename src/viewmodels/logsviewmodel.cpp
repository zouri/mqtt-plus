#include "viewmodels/logsviewmodel.h"

#include "models/eventstreammodel.h"
#include "usecases/eventhistoryservice.h"

#include <QStringList>

namespace {
QString logLevel(const QString &title, const QString &payload)
{
    const QString text = QStringLiteral("%1 %2").arg(title, payload).toUpper();
    if (text.contains(QStringLiteral("ERROR"))
            || text.contains(QStringLiteral("FAILED"))
            || text.contains(QStringLiteral("TIMEOUT"))
            || text.contains(QStringLiteral("REJECTED"))) {
        return QStringLiteral("ERROR");
    }
    if (text.contains(QStringLiteral("WARN")) || text.contains(QStringLiteral("INVALID"))) {
        return QStringLiteral("WARN");
    }
    if (text.contains(QStringLiteral("DEBUG")) || text.contains(QStringLiteral("PACKET"))) {
        return QStringLiteral("DEBUG");
    }
    return QStringLiteral("INFO");
}

QString indentedPayload(QString payload)
{
    return payload.replace(QLatin1Char('\n'), QStringLiteral("\n    "));
}
}

LogsViewModel::LogsViewModel(
    EventHistoryService &history,
    EventStreamModel &logs,
    QObject *parent)
    : QObject(parent)
    , m_history(history)
    , m_logs(logs)
{
    rebuildCachedText();

    connect(&m_history, &EventHistoryService::logStreamChanged,
        this, [this]() {
            rebuildCachedText();
            emit logTextChanged();
            emit logTextReset();
        });
    connect(&m_logs, &QAbstractItemModel::rowsInserted,
        this, [this](const QModelIndex &, int first, int last) {
            handleRowsInserted(first, last);
        });
    connect(&m_logs, &QAbstractItemModel::rowsAboutToBeRemoved,
        this, [this](const QModelIndex &, int first, int last) {
            handleRowsAboutToBeRemoved(first, last);
        });
    connect(&m_logs, &QAbstractItemModel::modelReset,
        this, [this]() {
            rebuildCachedText();
            emit logTextChanged();
            emit logTextReset();
        });
    connect(&m_logs, &QAbstractItemModel::dataChanged,
        this, [this]() {
            rebuildCachedText();
            emit logTextChanged();
            emit logTextReset();
        });
    connect(&m_logs, &QAbstractItemModel::layoutChanged,
        this, [this]() {
            rebuildCachedText();
            emit logTextChanged();
            emit logTextReset();
        });
}

EventStreamModel *LogsViewModel::logs() const
{
    return &m_logs;
}

QString LogsViewModel::logText() const
{
    return m_logText;
}

QString LogsViewModel::formattedLogRow(const QVariantMap &row)
{
    if (row.isEmpty()) {
        return QString();
    }
    if (row.value(QStringLiteral("kind")).toString() == QStringLiteral("divider")) {
        return QStringLiteral("--- %1 ---").arg(row.value(QStringLiteral("title")).toString());
    }

    const QString title = row.value(QStringLiteral("title")).toString();
    const QString payload = row.value(QStringLiteral("payload")).toString();
    const QString level = logLevel(title, payload);
    const QString channel = !title.isEmpty() && title.toUpper() != level
        ? QStringLiteral(" [%1]").arg(title)
        : QString();
    return QStringLiteral("[%1] [%2]%3 %4")
        .arg(
            row.value(QStringLiteral("timestamp")).toString(),
            level,
            channel,
            indentedPayload(payload));
}

QString LogsViewModel::renderedLogText(const EventStreamModel *model)
{
    if (!model) {
        return QString();
    }

    QStringList rows;
    rows.reserve(model->count());
    for (int row = 0; row < model->count(); ++row) {
        rows.append(formattedLogRow(model->rowAt(row)));
    }
    return rows.join(QLatin1Char('\n'));
}

void LogsViewModel::rebuildCachedText()
{
    QStringList rows;
    rows.reserve(m_logs.count());
    m_rowTextLengths.clear();
    m_rowTextLengths.reserve(m_logs.count());
    for (int row = 0; row < m_logs.count(); ++row) {
        const QString formatted = formattedLogRow(m_logs.rowAt(row));
        rows.append(formatted);
        m_rowTextLengths.append(formatted.size());
    }
    m_logText = rows.join(QLatin1Char('\n'));
}

void LogsViewModel::handleRowsInserted(int first, int last)
{
    const int insertedCount = last - first + 1;
    if (insertedCount <= 0
        || first < 0
        || first > m_rowTextLengths.size()
        || m_rowTextLengths.size() + insertedCount != m_logs.count()) {
        rebuildCachedText();
        emit logTextChanged();
        emit logTextReset();
        return;
    }

    QStringList rows;
    QVector<int> lengths;
    rows.reserve(insertedCount);
    lengths.reserve(insertedCount);
    for (int row = first; row <= last; ++row) {
        const QString formatted = formattedLogRow(m_logs.rowAt(row));
        rows.append(formatted);
        lengths.append(formatted.size());
    }

    const int oldRowCount = m_rowTextLengths.size();
    int position = 0;
    QString insertedText = rows.join(QLatin1Char('\n'));
    if (oldRowCount == 0) {
        position = 0;
    } else if (first == oldRowCount) {
        position = m_logText.size();
        insertedText.prepend(QLatin1Char('\n'));
    } else {
        position = rowStartPosition(first);
        insertedText.append(QLatin1Char('\n'));
    }

    for (int offset = 0; offset < lengths.size(); ++offset) {
        m_rowTextLengths.insert(first + offset, lengths.at(offset));
    }
    m_logText.insert(position, insertedText);
    emit logTextInserted(position, insertedText);
    emit logTextChanged();
}

void LogsViewModel::handleRowsAboutToBeRemoved(int first, int last)
{
    if (first < 0 || last < first || last >= m_rowTextLengths.size()) {
        rebuildCachedText();
        emit logTextChanged();
        emit logTextReset();
        return;
    }

    int start = rowStartPosition(first);
    int end = rowStartPosition(last) + m_rowTextLengths.at(last);
    if (first > 0) {
        --start;
    } else if (last + 1 < m_rowTextLengths.size()) {
        ++end;
    }

    m_rowTextLengths.remove(first, last - first + 1);
    m_logText.remove(start, end - start);
    emit logTextRemoved(start, end);
    emit logTextChanged();
}

int LogsViewModel::rowStartPosition(int row) const
{
    int position = row;
    for (int index = 0; index < row; ++index) {
        position += m_rowTextLengths.at(index);
    }
    return position;
}
