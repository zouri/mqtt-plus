#pragma once

#include <QString>

class ApplicationSessionRuntime;
class QSettings;
class ScriptController;
class SessionController;

class ApplicationSessionRepository
{
public:
    ApplicationSessionRepository(
        QSettings &settings,
        SessionController &sessionController,
        ScriptController &scriptController,
        ApplicationSessionRuntime &sessionRuntime);

    bool loadSessions(QString &errorMessage);
    bool saveSessions(QString &errorMessage);

private:
    QSettings &m_settings;
    SessionController &m_sessionController;
    ScriptController &m_scriptController;
    ApplicationSessionRuntime &m_sessionRuntime;
};
