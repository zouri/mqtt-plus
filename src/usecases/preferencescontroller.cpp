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

int sanitizePayloadLimit(const QVariant &value, int fallback)
{
    bool ok = false;
    const int bytes = value.toInt(&ok);
    if (!ok) {
        return fallback;
    }
    if (bytes <= 0) {
        return 0;
    }
    return (std::clamp)(bytes, 64 * 1024, 16 * 1024 * 1024);
}

int sanitizeSubscriptionPaneWidth(const QVariant &value, int fallback)
{
    bool ok = false;
    const int width = value.toInt(&ok);
    return ok ? (std::clamp)(width, 276, 520) : fallback;
}

int sanitizePublishComposerHeight(const QVariant &value, int fallback)
{
    bool ok = false;
    const int height = value.toInt(&ok);
    return ok ? (std::clamp)(height, 150, 300) : fallback;
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
    m_maxIncomingPayloadBytes = sanitizePayloadLimit(
        m_settings->value(QStringLiteral("history/maxIncomingPayloadBytes"), m_maxIncomingPayloadBytes),
        m_maxIncomingPayloadBytes);
    m_deleteHistoryWithSession =
        m_settings->value(QStringLiteral("history/deleteHistoryWithSession"), m_deleteHistoryWithSession).toBool();
    m_saveMessagesWhenOutputPaused =
        m_settings->value(QStringLiteral("history/saveMessagesWhenOutputPaused"), m_saveMessagesWhenOutputPaused).toBool();
    m_autoCollapseConnectionListOnConnect =
        m_settings->value(QStringLiteral("ui/autoCollapseConnectionListOnConnect"), m_autoCollapseConnectionListOnConnect).toBool();
    m_clearMessagesOnExit = sanitizeCleanupMode(
        m_settings->value(QStringLiteral("cleanup/clearMessagesOnExit"), m_clearMessagesOnExit).toString());
    m_clearLogsOnExit = sanitizeCleanupMode(
        m_settings->value(QStringLiteral("cleanup/clearLogsOnExit"), m_clearLogsOnExit).toString());
    const QSize storedWindowSize =
        m_settings->value(QStringLiteral("window/size"), m_windowSize).toSize();
    m_windowSize = storedWindowSize.isEmpty() ? QSize {} : storedWindowSize;
    m_windowMaximized = m_settings->value(QStringLiteral("window/maximized"), m_windowMaximized).toBool();
    m_subscriptionPaneWidth = sanitizeSubscriptionPaneWidth(
        m_settings->value(QStringLiteral("workspace/subscriptionPaneWidth"), m_subscriptionPaneWidth),
        m_subscriptionPaneWidth);
    m_publishComposerHeight = sanitizePublishComposerHeight(
        m_settings->value(QStringLiteral("workspace/publishComposerHeight"), m_publishComposerHeight),
        m_publishComposerHeight);
    m_connectionPaneCollapsed =
        m_settings->value(QStringLiteral("workspace/connectionPaneCollapsed"), m_connectionPaneCollapsed).toBool();
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

int PreferencesController::maxIncomingPayloadBytes() const
{
    return m_maxIncomingPayloadBytes;
}

bool PreferencesController::deleteHistoryWithSession() const
{
    return m_deleteHistoryWithSession;
}

bool PreferencesController::saveMessagesWhenOutputPaused() const
{
    return m_saveMessagesWhenOutputPaused;
}

bool PreferencesController::autoCollapseConnectionListOnConnect() const
{
    return m_autoCollapseConnectionListOnConnect;
}

QString PreferencesController::clearMessagesOnExit() const
{
    return m_clearMessagesOnExit;
}

QString PreferencesController::clearLogsOnExit() const
{
    return m_clearLogsOnExit;
}

QSize PreferencesController::windowSize() const
{
    return m_windowSize;
}

bool PreferencesController::windowMaximized() const
{
    return m_windowMaximized;
}

int PreferencesController::subscriptionPaneWidth() const
{
    return m_subscriptionPaneWidth;
}

int PreferencesController::publishComposerHeight() const
{
    return m_publishComposerHeight;
}

bool PreferencesController::connectionPaneCollapsed() const
{
    return m_connectionPaneCollapsed;
}

QVariantMap PreferencesController::portableSettings() const
{
    return {
        {QStringLiteral("history/messageRetentionLimit"), m_messageRetentionLimit},
        {QStringLiteral("history/logRetentionLimit"), m_logRetentionLimit},
        {QStringLiteral("history/pageSize"), m_historyPageSize},
        {QStringLiteral("history/maxIncomingPayloadBytes"), m_maxIncomingPayloadBytes},
        {QStringLiteral("history/deleteHistoryWithSession"), m_deleteHistoryWithSession},
        {QStringLiteral("history/saveMessagesWhenOutputPaused"), m_saveMessagesWhenOutputPaused},
        {QStringLiteral("ui/autoCollapseConnectionListOnConnect"), m_autoCollapseConnectionListOnConnect},
        {QStringLiteral("cleanup/clearMessagesOnExit"), m_clearMessagesOnExit},
        {QStringLiteral("cleanup/clearLogsOnExit"), m_clearLogsOnExit},
    };
}

bool PreferencesController::applyPortableSettings(
    const QVariantMap &settings,
    QString &errorMessage)
{
    errorMessage.clear();
    if (settings.contains(QStringLiteral("history/messageRetentionLimit"))) {
        setMessageRetentionLimit(settings.value(QStringLiteral("history/messageRetentionLimit")).toInt());
    }
    if (settings.contains(QStringLiteral("history/logRetentionLimit"))) {
        setLogRetentionLimit(settings.value(QStringLiteral("history/logRetentionLimit")).toInt());
    }
    if (settings.contains(QStringLiteral("history/pageSize"))) {
        setHistoryPageSize(settings.value(QStringLiteral("history/pageSize")).toInt());
    }
    if (settings.contains(QStringLiteral("history/maxIncomingPayloadBytes"))) {
        setMaxIncomingPayloadBytes(settings.value(QStringLiteral("history/maxIncomingPayloadBytes")).toInt());
    }
    if (settings.contains(QStringLiteral("history/deleteHistoryWithSession"))) {
        setDeleteHistoryWithSession(settings.value(QStringLiteral("history/deleteHistoryWithSession")).toBool());
    }
    if (settings.contains(QStringLiteral("history/saveMessagesWhenOutputPaused"))) {
        setSaveMessagesWhenOutputPaused(settings.value(QStringLiteral("history/saveMessagesWhenOutputPaused")).toBool());
    }
    if (settings.contains(QStringLiteral("ui/autoCollapseConnectionListOnConnect"))) {
        setAutoCollapseConnectionListOnConnect(settings.value(QStringLiteral("ui/autoCollapseConnectionListOnConnect")).toBool());
    }
    if (settings.contains(QStringLiteral("cleanup/clearMessagesOnExit"))) {
        setClearMessagesOnExit(settings.value(QStringLiteral("cleanup/clearMessagesOnExit")).toString());
    }
    if (settings.contains(QStringLiteral("cleanup/clearLogsOnExit"))) {
        setClearLogsOnExit(settings.value(QStringLiteral("cleanup/clearLogsOnExit")).toString());
    }
    if (!m_settings) {
        return true;
    }
    m_settings->sync();
    if (m_settings->status() == QSettings::NoError) {
        return true;
    }
    errorMessage = m_settings->status() == QSettings::AccessError
        ? tr("Cannot write imported preferences: access denied.")
        : tr("Cannot write imported preferences: invalid settings format.");
    return false;
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

void PreferencesController::setMaxIncomingPayloadBytes(int bytes)
{
    const int sanitized = sanitizePayloadLimit(bytes, m_maxIncomingPayloadBytes);
    if (sanitized == m_maxIncomingPayloadBytes) {
        return;
    }

    m_maxIncomingPayloadBytes = sanitized;
    syncValue(QStringLiteral("history/maxIncomingPayloadBytes"), m_maxIncomingPayloadBytes);
    emit maxIncomingPayloadBytesChanged();
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

void PreferencesController::setAutoCollapseConnectionListOnConnect(bool enabled)
{
    if (enabled == m_autoCollapseConnectionListOnConnect) {
        return;
    }

    m_autoCollapseConnectionListOnConnect = enabled;
    syncValue(QStringLiteral("ui/autoCollapseConnectionListOnConnect"), m_autoCollapseConnectionListOnConnect);
    emit autoCollapseConnectionListOnConnectChanged();
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

void PreferencesController::setWindowState(const QSize &size, bool maximized)
{
    if (size.isEmpty()
        || (size == m_windowSize && maximized == m_windowMaximized)) {
        return;
    }

    m_windowSize = size;
    m_windowMaximized = maximized;
    if (m_settings) {
        m_settings->setValue(QStringLiteral("window/size"), m_windowSize);
        m_settings->setValue(QStringLiteral("window/maximized"), m_windowMaximized);
        m_settings->sync();
    }
}

void PreferencesController::setWorkbenchLayout(
    int subscriptionPaneWidth,
    int publishComposerHeight,
    bool connectionPaneCollapsed)
{
    const int sanitizedPaneWidth = sanitizeSubscriptionPaneWidth(subscriptionPaneWidth, m_subscriptionPaneWidth);
    const int sanitizedComposerHeight = sanitizePublishComposerHeight(publishComposerHeight, m_publishComposerHeight);
    if (sanitizedPaneWidth == m_subscriptionPaneWidth
        && sanitizedComposerHeight == m_publishComposerHeight
        && connectionPaneCollapsed == m_connectionPaneCollapsed) {
        return;
    }

    m_subscriptionPaneWidth = sanitizedPaneWidth;
    m_publishComposerHeight = sanitizedComposerHeight;
    m_connectionPaneCollapsed = connectionPaneCollapsed;
    if (m_settings) {
        m_settings->setValue(QStringLiteral("workspace/subscriptionPaneWidth"), m_subscriptionPaneWidth);
        m_settings->setValue(QStringLiteral("workspace/publishComposerHeight"), m_publishComposerHeight);
        m_settings->setValue(QStringLiteral("workspace/connectionPaneCollapsed"), m_connectionPaneCollapsed);
        m_settings->sync();
    }
    emit workbenchLayoutChanged();
}

void PreferencesController::syncValue(const QString &key, const QVariant &value)
{
    if (!m_settings) {
        return;
    }

    m_settings->setValue(key, value);
    m_settings->sync();
}
