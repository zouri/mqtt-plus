#include "models/notificationcentermodel.h"

#include <QStringList>

#include <algorithm>

NotificationCenterModel::NotificationCenterModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_tickTimer.setInterval(100);
    m_tickTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_tickTimer, &QTimer::timeout, this, &NotificationCenterModel::tick);
    m_tickTimer.start();
}

int NotificationCenterModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visible.size();
}

int NotificationCenterModel::count() const { return rowCount(); }

QVariant NotificationCenterModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size()) {
        return {};
    }
    const Entry &entry = m_visible.at(index.row());
    switch (role) {
    case IdRole: return entry.id;
    case TitleRole: return entry.title;
    case MessageRole: return entry.message;
    case SeverityRole: return entry.severity;
    case ActionLabelRole: return entry.actionLabel;
    case ActionIdRole: return entry.actionId;
    default: return {};
    }
}

QHash<int, QByteArray> NotificationCenterModel::roleNames() const
{
    return {
        {IdRole, "notificationId"},
        {TitleRole, "title"},
        {MessageRole, "message"},
        {SeverityRole, "severity"},
        {ActionLabelRole, "actionLabel"},
        {ActionIdRole, "actionId"},
    };
}

void NotificationCenterModel::postOrUpdate(
    const QString &id,
    const QString &title,
    const QString &message,
    const QString &severity,
    int autoCloseMs,
    const QString &actionLabel,
    const QString &actionId)
{
    const int visibleRow = visibleIndex(id);
    if (visibleRow >= 0) {
        updateEntry(m_visible[visibleRow], title, message, severity, autoCloseMs, actionLabel, actionId);
        emit dataChanged(index(visibleRow, 0), index(visibleRow, 0));
        return;
    }
    const int queuedRow = queuedIndex(id);
    if (queuedRow >= 0) {
        updateEntry(m_queued[queuedRow], title, message, severity, autoCloseMs, actionLabel, actionId);
        return;
    }

    Entry entry;
    entry.id = id;
    updateEntry(entry, title, message, severity, autoCloseMs, actionLabel, actionId);
    if (m_visible.size() < kMaxVisible) {
        const int row = m_visible.size();
        beginInsertRows(QModelIndex(), row, row);
        m_visible.append(entry);
        endInsertRows();
        emit countChanged();
    } else {
        if (m_queued.size() >= kMaxQueued) {
            const auto disposable = std::find_if(
                m_queued.begin(),
                m_queued.end(),
                [](const Entry &queued) {
                    return queued.remainingMs > 0
                        && queued.actionId.isEmpty()
                        && queued.severity != QStringLiteral("error");
                });
            if (disposable != m_queued.end()) {
                m_queued.erase(disposable);
            } else if (entry.remainingMs > 0
                       && entry.actionId.isEmpty()
                       && entry.severity != QStringLiteral("error")) {
                return;
            } else {
                m_queued.removeFirst();
            }
        }
        m_queued.append(entry);
    }
}

void NotificationCenterModel::dismiss(const QString &id)
{
    const int row = visibleIndex(id);
    if (row >= 0) {
        beginRemoveRows(QModelIndex(), row, row);
        m_visible.removeAt(row);
        endRemoveRows();
        emit countChanged();
        promoteQueued();
        return;
    }
    const int queuedRow = queuedIndex(id);
    if (queuedRow >= 0) {
        m_queued.removeAt(queuedRow);
    }
}

void NotificationCenterModel::setHovered(const QString &id, bool hovered)
{
    const int row = visibleIndex(id);
    if (row >= 0) {
        m_visible[row].hovered = hovered;
    }
}

void NotificationCenterModel::triggerAction(const QString &id)
{
    const int row = visibleIndex(id);
    if (row < 0) {
        return;
    }
    const QString actionId = m_visible.at(row).actionId;
    if (!actionId.isEmpty()) {
        emit actionRequested(actionId);
    }
    dismiss(id);
}

int NotificationCenterModel::visibleIndex(const QString &id) const
{
    for (qsizetype index = 0; index < m_visible.size(); ++index) {
        if (m_visible.at(index).id == id) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int NotificationCenterModel::queuedIndex(const QString &id) const
{
    for (qsizetype index = 0; index < m_queued.size(); ++index) {
        if (m_queued.at(index).id == id) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

void NotificationCenterModel::updateEntry(
    Entry &entry,
    const QString &title,
    const QString &message,
    const QString &severity,
    int autoCloseMs,
    const QString &actionLabel,
    const QString &actionId)
{
    entry.title = title;
    entry.message = message;
    entry.severity = severity;
    entry.actionLabel = actionLabel;
    entry.actionId = actionId;
    entry.remainingMs = (std::max)(0, autoCloseMs);
}

void NotificationCenterModel::promoteQueued()
{
    if (m_visible.size() >= kMaxVisible || m_queued.isEmpty()) {
        return;
    }
    const int row = m_visible.size();
    beginInsertRows(QModelIndex(), row, row);
    m_visible.append(m_queued.takeFirst());
    endInsertRows();
    emit countChanged();
}

void NotificationCenterModel::tick()
{
    QStringList expired;
    for (Entry &entry : m_visible) {
        if (entry.hovered || entry.remainingMs <= 0) {
            continue;
        }
        entry.remainingMs -= m_tickTimer.interval();
        if (entry.remainingMs <= 0) {
            expired.append(entry.id);
        }
    }
    for (const QString &id : expired) {
        dismiss(id);
    }
}
