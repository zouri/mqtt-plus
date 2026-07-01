#include "viewmodels/logsviewmodel.h"

#include "models/eventstreammodel.h"
#include "viewmodels/logscoreport.h"

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

LogsViewModel::LogsViewModel(LogsCorePort *core, QObject *parent)
    : QObject(parent)
    , m_core(core)
{
    if (m_core) {
        m_core->bindLogsSignals(this, {
            [this]() {
                emit logTextChanged();
                emit logStreamChanged();
            },
            [this](const QVariantMap &row) {
                emit logTextChanged();
                emit logStreamRowAppended(row);
            },
        });
    }
    if (auto *model = logs()) {
        connect(model, &EventStreamModel::countChanged, this, &LogsViewModel::logTextChanged);
    }
}

EventStreamModel *LogsViewModel::logs() const
{
    return m_core ? m_core->logs() : nullptr;
}

QString LogsViewModel::logText() const
{
    return renderedLogText(logs());
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

void LogsViewModel::clearCurrentLogs()
{
    if (m_core) {
        m_core->clearCurrentLogs();
    }
}

int LogsViewModel::loadOlderCurrentSessionLogs()
{
    return m_core ? m_core->loadOlderCurrentSessionLogs() : 0;
}
