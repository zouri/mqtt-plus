#include "preferencescontroller.h"

#include <QVariant>

#include <algorithm>

namespace {
int sanitizeRetentionLimit(const QVariant &value, int fallback)
{
    bool ok = false;
    const int limit = value.toInt(&ok);
    if (!ok) {
        return fallback;
    }
    if (limit <= 0) {
        return 0;
    }
    return (std::clamp)(limit, 100, 1000000);
}

int sanitizePageSize(const QVariant &value, int fallback)
{
    bool ok = false;
    const int pageSize = value.toInt(&ok);
    if (!ok) {
        return fallback;
    }
    return (std::clamp)(pageSize, 50, 5000);
}

QString sanitizeCleanupMode(const QString &value)
{
    const QString mode = value.trimmed();
    if (mode == QStringLiteral("current") || mode == QStringLiteral("all")) {
        return mode;
    }
    return QStringLiteral("never");
}
}

PreferencesController::PreferencesController(QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    if (!m_settings) {
        return;
    }

    m_messageRetentionLimit = sanitizeRetentionLimit(
        m_settings->value(QStringLiteral("history/messageRetentionLimit"), m_messageRetentionLimit),
        m_messageRetentionLimit);
    m_logRetentionLimit = sanitizeRetentionLimit(
        m_settings->value(QStringLiteral("history/logRetentionLimit"), m_logRetentionLimit),
        m_logRetentionLimit);
    m_historyPageSize = sanitizePageSize(
        m_settings->value(QStringLiteral("history/pageSize"), m_historyPageSize),
        m_historyPageSize);
    m_deleteHistoryWithSession =
        m_settings->value(QStringLiteral("history/deleteHistoryWithSession"), m_deleteHistoryWithSession).toBool();
    m_saveMessagesWhenOutputPaused =
        m_settings->value(QStringLiteral("history/saveMessagesWhenOutputPaused"), m_saveMessagesWhenOutputPaused).toBool();
    m_clearMessagesOnExit = sanitizeCleanupMode(
        m_settings->value(QStringLiteral("cleanup/clearMessagesOnExit"), m_clearMessagesOnExit).toString());
    m_clearLogsOnExit = sanitizeCleanupMode(
        m_settings->value(QStringLiteral("cleanup/clearLogsOnExit"), m_clearLogsOnExit).toString());
}

int PreferencesController::messageRetentionLimit() const
{
    return m_messageRetentionLimit;
}

int PreferencesController::logRetentionLimit() const
{
    return m_logRetentionLimit;
}

int PreferencesController::historyPageSize() const
{
    return m_historyPageSize;
}

bool PreferencesController::deleteHistoryWithSession() const
{
    return m_deleteHistoryWithSession;
}

bool PreferencesController::saveMessagesWhenOutputPaused() const
{
    return m_saveMessagesWhenOutputPaused;
}

QString PreferencesController::clearMessagesOnExit() const
{
    return m_clearMessagesOnExit;
}

QString PreferencesController::clearLogsOnExit() const
{
    return m_clearLogsOnExit;
}

void PreferencesController::setMessageRetentionLimit(int limit)
{
    const int sanitized = sanitizeRetentionLimit(limit, m_messageRetentionLimit);
    if (sanitized == m_messageRetentionLimit) {
        return;
    }

    m_messageRetentionLimit = sanitized;
    syncValue(QStringLiteral("history/messageRetentionLimit"), m_messageRetentionLimit);
    emit messageRetentionLimitChanged();
}

void PreferencesController::setLogRetentionLimit(int limit)
{
    const int sanitized = sanitizeRetentionLimit(limit, m_logRetentionLimit);
    if (sanitized == m_logRetentionLimit) {
        return;
    }

    m_logRetentionLimit = sanitized;
    syncValue(QStringLiteral("history/logRetentionLimit"), m_logRetentionLimit);
    emit logRetentionLimitChanged();
}

void PreferencesController::setHistoryPageSize(int pageSize)
{
    const int sanitized = sanitizePageSize(pageSize, m_historyPageSize);
    if (sanitized == m_historyPageSize) {
        return;
    }

    m_historyPageSize = sanitized;
    syncValue(QStringLiteral("history/pageSize"), m_historyPageSize);
    emit historyPageSizeChanged();
}

void PreferencesController::setDeleteHistoryWithSession(bool enabled)
{
    if (enabled == m_deleteHistoryWithSession) {
        return;
    }

    m_deleteHistoryWithSession = enabled;
    syncValue(QStringLiteral("history/deleteHistoryWithSession"), m_deleteHistoryWithSession);
    emit deleteHistoryWithSessionChanged();
}

void PreferencesController::setSaveMessagesWhenOutputPaused(bool enabled)
{
    if (enabled == m_saveMessagesWhenOutputPaused) {
        return;
    }

    m_saveMessagesWhenOutputPaused = enabled;
    syncValue(QStringLiteral("history/saveMessagesWhenOutputPaused"), m_saveMessagesWhenOutputPaused);
    emit saveMessagesWhenOutputPausedChanged();
}

void PreferencesController::setClearMessagesOnExit(const QString &mode)
{
    const QString sanitized = sanitizeCleanupMode(mode);
    if (sanitized == m_clearMessagesOnExit) {
        return;
    }

    m_clearMessagesOnExit = sanitized;
    syncValue(QStringLiteral("cleanup/clearMessagesOnExit"), m_clearMessagesOnExit);
    emit clearMessagesOnExitChanged();
}

void PreferencesController::setClearLogsOnExit(const QString &mode)
{
    const QString sanitized = sanitizeCleanupMode(mode);
    if (sanitized == m_clearLogsOnExit) {
        return;
    }

    m_clearLogsOnExit = sanitized;
    syncValue(QStringLiteral("cleanup/clearLogsOnExit"), m_clearLogsOnExit);
    emit clearLogsOnExitChanged();
}

void PreferencesController::syncValue(const QString &key, const QVariant &value)
{
    if (!m_settings) {
        return;
    }

    m_settings->setValue(key, value);
    m_settings->sync();
}
