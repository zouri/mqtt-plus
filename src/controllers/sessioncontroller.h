#pragma once

#include "domain/session.h"

#include <QObject>
#include <QVariantMap>
#include <QVector>

class AppFacade;

class SessionController : public QObject
{
    Q_OBJECT

public:
    explicit SessionController(QObject *parent = nullptr);

    void setFacade(AppFacade *app);

    QVector<SessionState> &sessions();
    const QVector<SessionState> &sessions() const;
    int currentIndex() const;
    void setCurrentIndex(int index);

    SessionState *currentSession();
    const SessionState *currentSession() const;
    SessionState *sessionById(const QString &sessionId);
    const SessionState *sessionById(const QString &sessionId) const;

    void appendSession(const SessionState &session);
    SessionState takeSessionAt(int index);
    void clear();
    bool isValidIndex(int index) const;

    void setCurrentSessionIndex(int index);
    QVariantMap defaultSessionConfig() const;
    QVariantMap sessionConfigAt(int index) const;
    bool updateSessionConfigAt(int index, const QVariantMap &config);
    void addSessionWithConfig(const QVariantMap &config);
    void duplicateSessionAt(int index);
    void removeSessionAt(int index);
    void setCurrentOutputPaused(bool paused);

private:
    AppFacade *m_app = nullptr;
    QVector<SessionState> m_sessions;
    int m_currentIndex = -1;
};
