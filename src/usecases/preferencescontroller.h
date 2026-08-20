#pragma once

#include <QObject>
#include <QSettings>
#include <QSize>
#include <QString>
#include <QVariant>
#include <QVariantMap>

class PreferencesController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool deleteHistoryWithSession READ deleteHistoryWithSession WRITE setDeleteHistoryWithSession NOTIFY deleteHistoryWithSessionChanged)
    Q_PROPERTY(bool saveMessagesWhenOutputPaused READ saveMessagesWhenOutputPaused WRITE setSaveMessagesWhenOutputPaused NOTIFY saveMessagesWhenOutputPausedChanged)
    Q_PROPERTY(bool autoCollapseConnectionListOnConnect READ autoCollapseConnectionListOnConnect WRITE setAutoCollapseConnectionListOnConnect NOTIFY autoCollapseConnectionListOnConnectChanged)
    Q_PROPERTY(int autoFollowFps READ autoFollowFps WRITE setAutoFollowFps NOTIFY autoFollowFpsChanged)
    Q_PROPERTY(int subscriptionPaneWidth READ subscriptionPaneWidth NOTIFY workbenchLayoutChanged)
    Q_PROPERTY(int publishComposerHeight READ publishComposerHeight NOTIFY workbenchLayoutChanged)
    Q_PROPERTY(bool connectionPaneCollapsed READ connectionPaneCollapsed NOTIFY workbenchLayoutChanged)
    Q_PROPERTY(QString workbenchContextPane READ workbenchContextPane WRITE setWorkbenchContextPane NOTIFY workbenchContextPaneChanged)

public:
    explicit PreferencesController(QSettings *settings, QObject *parent = nullptr);

    int messageRetentionLimit() const;
    int logRetentionLimit() const;
    int historyPageSize() const;
    int maxIncomingPayloadBytes() const;
    bool deleteHistoryWithSession() const;
    bool saveMessagesWhenOutputPaused() const;
    bool autoCollapseConnectionListOnConnect() const;
    int autoFollowFps() const;
    QString clearMessagesOnExit() const;
    QString clearLogsOnExit() const;
    QSize windowSize() const;
    bool windowMaximized() const;
    int subscriptionPaneWidth() const;
    int publishComposerHeight() const;
    bool connectionPaneCollapsed() const;
    QString workbenchContextPane() const;
    QVariantMap portableSettings() const;
    bool applyPortableSettings(const QVariantMap &settings, QString &errorMessage);
    void setWindowState(const QSize &size, bool maximized);

public slots:
    void setMessageRetentionLimit(int limit);
    void setLogRetentionLimit(int limit);
    void setHistoryPageSize(int pageSize);
    void setMaxIncomingPayloadBytes(int bytes);
    void setDeleteHistoryWithSession(bool enabled);
    void setSaveMessagesWhenOutputPaused(bool enabled);
    void setAutoCollapseConnectionListOnConnect(bool enabled);
    void setAutoFollowFps(int fps);
    void setClearMessagesOnExit(const QString &mode);
    void setClearLogsOnExit(const QString &mode);
    void setWorkbenchLayout(
        int subscriptionPaneWidth,
        int publishComposerHeight,
        bool connectionPaneCollapsed);
    void setWorkbenchContextPane(const QString &pane);

signals:
    void messageRetentionLimitChanged();
    void logRetentionLimitChanged();
    void historyPageSizeChanged();
    void maxIncomingPayloadBytesChanged();
    void deleteHistoryWithSessionChanged();
    void saveMessagesWhenOutputPausedChanged();
    void autoCollapseConnectionListOnConnectChanged();
    void autoFollowFpsChanged();
    void clearMessagesOnExitChanged();
    void clearLogsOnExitChanged();
    void workbenchLayoutChanged();
    void workbenchContextPaneChanged();

private:
    void syncValue(const QString &key, const QVariant &value);

    QSettings *m_settings = nullptr;
    int m_messageRetentionLimit = 5000;
    int m_logRetentionLimit = 2000;
    int m_historyPageSize = 500;
    int m_maxIncomingPayloadBytes = 1024 * 1024;
    bool m_deleteHistoryWithSession = true;
    bool m_saveMessagesWhenOutputPaused = true;
    bool m_autoCollapseConnectionListOnConnect = true;
    int m_autoFollowFps = 30;
    QString m_clearMessagesOnExit = QStringLiteral("never");
    QString m_clearLogsOnExit = QStringLiteral("never");
    QSize m_windowSize;
    bool m_windowMaximized = false;
    int m_subscriptionPaneWidth = 320;
    int m_publishComposerHeight = 168;
    bool m_connectionPaneCollapsed = false;
    QString m_workbenchContextPane = QStringLiteral("topics");
};
