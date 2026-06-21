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
    int maxIncomingPayloadBytes() const;
    bool deleteHistoryWithSession() const;
    bool saveMessagesWhenOutputPaused() const;
    QString clearMessagesOnExit() const;
    QString clearLogsOnExit() const;

public slots:
    void setMessageRetentionLimit(int limit);
    void setLogRetentionLimit(int limit);
    void setHistoryPageSize(int pageSize);
    void setMaxIncomingPayloadBytes(int bytes);
    void setDeleteHistoryWithSession(bool enabled);
    void setSaveMessagesWhenOutputPaused(bool enabled);
    void setClearMessagesOnExit(const QString &mode);
    void setClearLogsOnExit(const QString &mode);

signals:
    void messageRetentionLimitChanged();
    void logRetentionLimitChanged();
    void historyPageSizeChanged();
    void maxIncomingPayloadBytesChanged();
    void deleteHistoryWithSessionChanged();
    void saveMessagesWhenOutputPausedChanged();
    void clearMessagesOnExitChanged();
    void clearLogsOnExitChanged();

private:
    void syncValue(const QString &key, const QVariant &value);

    QSettings *m_settings = nullptr;
    int m_messageRetentionLimit = 5000;
    int m_logRetentionLimit = 2000;
    int m_historyPageSize = 500;
    int m_maxIncomingPayloadBytes = 1024 * 1024;
    bool m_deleteHistoryWithSession = true;
    bool m_saveMessagesWhenOutputPaused = true;
    QString m_clearMessagesOnExit = QStringLiteral("never");
    QString m_clearLogsOnExit = QStringLiteral("never");
};
