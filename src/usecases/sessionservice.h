#pragma once

#include "domain/session.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

class HistoryStore;
class HistoryWriterWorker;
class MessageParseWorker;
class PreferencesController;
class QSettings;
struct MessageCapturePolicy;

struct SessionImportRequest {
    QString id;
    QVariantMap config;
    QVector<SubscriptionEntry> subscriptions;
    bool outputPaused = false;
    bool captureIncoming = true;
    bool captureOutgoing = true;
    QStringList captureIncludeTopicFilters;
    QStringList captureExcludeTopicFilters;
};

class SessionService : public QObject
{
    Q_OBJECT

public:
    explicit SessionService(
        QSettings &settings,
        HistoryStore &historyStore,
        PreferencesController &preferences,
        QObject *parent = nullptr);

    QVector<SessionState> &sessions();
    const QVector<SessionState> &sessions() const;
    int currentIndex() const;

    SessionState *currentSession();
    const SessionState *currentSession() const;
    SessionState *sessionById(const QString &sessionId);
    const SessionState *sessionById(const QString &sessionId) const;
    MessageCapturePolicy messageCapturePolicy(const QString &sessionId) const;
    bool setMessageCapturePolicy(
        const QString &sessionId,
        const MessageCapturePolicy &policy);

    bool loadSessions();
    bool saveSessions();
    Q_INVOKABLE void setCurrentSessionIndex(int index);
    QVariantMap defaultSessionConfig() const;
    QVariantMap sessionConfigAt(int index) const;
    bool updateSessionConfigAt(int index, const QVariantMap &config);
    bool addSessionWithConfig(const QVariantMap &config);
    bool importSessions(
        const QVector<SessionImportRequest> &requests,
        QStringList &importedSessionIds,
        QString &errorMessage);
    bool rollbackImportedSessions(
        const QStringList &sessionIds,
        QString &errorMessage);
    void duplicateSessionAt(int index);
    void removeSessionAt(int index);
    void setHistoryWriter(HistoryWriterWorker *historyWriter);
    void setMessageParser(MessageParseWorker *messageParser);
    Q_INVOKABLE void setCurrentOutputPaused(bool paused);

signals:
    void sessionsChanged();
    void currentSessionIndexChanged();
    void currentSessionChanged();
    void messageCapturePolicyChanged(const QString &sessionId);
    void currentSessionHistoryReloadRequested();
    void sessionRuntimeReady(SessionState *session);
    void reconnectRequested(SessionState *session);
    void runtimeError(
        const QString &sessionId,
        const QString &channel,
        const QString &message);
    void storageError(const QString &message);

private:
    bool isValidIndex(int index) const;
    void applyConfig(SessionState &session, const QVariantMap &config, bool keepNameFallback) const;
    void requestReconnect(SessionState &session);
    void initializeSessionRuntime(SessionState &session);
    void destroySessionRuntime(SessionState &session);
    SessionState createDefaultSession(const QString &name);

    QSettings &m_settings;
    HistoryStore &m_historyStore;
    HistoryWriterWorker *m_historyWriter = nullptr;
    MessageParseWorker *m_messageParser = nullptr;
    PreferencesController &m_preferences;
    QVector<SessionState> m_sessions;
    int m_currentIndex = -1;
};
