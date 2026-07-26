#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>

class PreferencesController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool deleteHistoryWithSession READ deleteHistoryWithSession WRITE setDeleteHistoryWithSession NOTIFY deleteHistoryWithSessionChanged)
    Q_PROPERTY(bool saveMessagesWhenOutputPaused READ saveMessagesWhenOutputPaused WRITE setSaveMessagesWhenOutputPaused NOTIFY saveMessagesWhenOutputPausedChanged)
    Q_PROPERTY(bool autoCollapseConnectionListOnConnect READ autoCollapseConnectionListOnConnect WRITE setAutoCollapseConnectionListOnConnect NOTIFY autoCollapseConnectionListOnConnectChanged)
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY windowHeightChanged)
    Q_PROPERTY(bool windowMaximized READ windowMaximized WRITE setWindowMaximized NOTIFY windowMaximizedChanged)
    Q_PROPERTY(int subscriptionPaneWidth READ subscriptionPaneWidth NOTIFY workbenchLayoutChanged)
    Q_PROPERTY(int publishComposerHeight READ publishComposerHeight NOTIFY workbenchLayoutChanged)
    Q_PROPERTY(bool connectionPaneCollapsed READ connectionPaneCollapsed NOTIFY workbenchLayoutChanged)

public:
    explicit PreferencesController(QSettings *settings, QObject *parent = nullptr);

    int messageRetentionLimit() const;
    int logRetentionLimit() const;
    int historyPageSize() const;
    int maxIncomingPayloadBytes() const;
    bool deleteHistoryWithSession() const;
    bool saveMessagesWhenOutputPaused() const;
    bool autoCollapseConnectionListOnConnect() const;
    QString clearMessagesOnExit() const;
    QString clearLogsOnExit() const;
    int windowWidth() const;
    int windowHeight() const;
    bool windowMaximized() const;
    int subscriptionPaneWidth() const;
    int publishComposerHeight() const;
    bool connectionPaneCollapsed() const;

public slots:
    void setMessageRetentionLimit(int limit);
    void setLogRetentionLimit(int limit);
    void setHistoryPageSize(int pageSize);
    void setMaxIncomingPayloadBytes(int bytes);
    void setDeleteHistoryWithSession(bool enabled);
    void setSaveMessagesWhenOutputPaused(bool enabled);
    void setAutoCollapseConnectionListOnConnect(bool enabled);
    void setClearMessagesOnExit(const QString &mode);
    void setClearLogsOnExit(const QString &mode);
    void setWindowGeometry(int width, int height);
    void setWindowMaximized(bool maximized);
    void setWorkbenchLayout(
        int subscriptionPaneWidth,
        int publishComposerHeight,
        bool connectionPaneCollapsed);

signals:
    void messageRetentionLimitChanged();
    void logRetentionLimitChanged();
    void historyPageSizeChanged();
    void maxIncomingPayloadBytesChanged();
    void deleteHistoryWithSessionChanged();
    void saveMessagesWhenOutputPausedChanged();
    void autoCollapseConnectionListOnConnectChanged();
    void clearMessagesOnExitChanged();
    void clearLogsOnExitChanged();
    void windowWidthChanged();
    void windowHeightChanged();
    void windowMaximizedChanged();
    void workbenchLayoutChanged();

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
    QString m_clearMessagesOnExit = QStringLiteral("never");
    QString m_clearLogsOnExit = QStringLiteral("never");
    int m_windowWidth = 1480;
    int m_windowHeight = 820;
    bool m_windowMaximized = false;
    int m_subscriptionPaneWidth = 320;
    int m_publishComposerHeight = 168;
    bool m_connectionPaneCollapsed = false;
};
