#pragma once

#include "domain/session.h"

#include <QObject>
#include <QVariantMap>
#include <QVector>

class HistoryStore;
class PreferencesController;
class QSettings;
class ScriptService;

class SessionService : public QObject
{
    Q_OBJECT

public:
    explicit SessionService(
        QSettings &settings,
        ScriptService &scriptService,
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

    bool loadSessions();
    bool saveSessions();
    void setCurrentSessionIndex(int index);
    QVariantMap defaultSessionConfig() const;
    QVariantMap sessionConfigAt(int index) const;
    bool updateSessionConfigAt(int index, const QVariantMap &config);
    bool addSessionWithConfig(const QVariantMap &config);
    void duplicateSessionAt(int index);
    void removeSessionAt(int index);
    void setCurrentOutputPaused(bool paused);

signals:
    void sessionsChanged();
    void currentSessionIndexChanged();
    void currentSessionChanged();
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
    ScriptService &m_scriptService;
    HistoryStore &m_historyStore;
    PreferencesController &m_preferences;
    QVector<SessionState> m_sessions;
    int m_currentIndex = -1;
};
