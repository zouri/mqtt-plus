#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>

class PreferencesController : public QObject
{
    Q_OBJECT

public:
    explicit PreferencesController(QSettings *settings, QObject *parent = nullptr);

    int messageRetentionLimit() const;
    int logRetentionLimit() const;
    int historyPageSize() const;
    bool deleteHistoryWithSession() const;
    bool saveMessagesWhenOutputPaused() const;
    QString clearMessagesOnExit() const;
    QString clearLogsOnExit() const;
    int windowWidth() const;
    int windowHeight() const;
    bool windowMaximized() const;

public slots:
    void setMessageRetentionLimit(int limit);
    void setLogRetentionLimit(int limit);
    void setHistoryPageSize(int pageSize);
    void setDeleteHistoryWithSession(bool enabled);
    void setSaveMessagesWhenOutputPaused(bool enabled);
    void setClearMessagesOnExit(const QString &mode);
    void setClearLogsOnExit(const QString &mode);
    void setWindowGeometry(int width, int height);
    void setWindowMaximized(bool maximized);

signals:
    void messageRetentionLimitChanged();
    void logRetentionLimitChanged();
    void historyPageSizeChanged();
    void deleteHistoryWithSessionChanged();
    void saveMessagesWhenOutputPausedChanged();
    void clearMessagesOnExitChanged();
    void clearLogsOnExitChanged();
    void windowWidthChanged();
    void windowHeightChanged();
    void windowMaximizedChanged();

private:
    void syncValue(const QString &key, const QVariant &value);

    QSettings *m_settings = nullptr;
    int m_messageRetentionLimit = 5000;
    int m_logRetentionLimit = 2000;
    int m_historyPageSize = 500;
    bool m_deleteHistoryWithSession = true;
    bool m_saveMessagesWhenOutputPaused = true;
    QString m_clearMessagesOnExit = QStringLiteral("never");
    QString m_clearLogsOnExit = QStringLiteral("never");
    int m_windowWidth = 1480;
    int m_windowHeight = 820;
    bool m_windowMaximized = false;
};
