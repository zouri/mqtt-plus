#pragma once

#include <QString>

class ApplicationSessionRuntime;
class QSettings;
class ScriptService;
class SessionService;

class ApplicationSessionRepository
{
public:
    ApplicationSessionRepository(
        QSettings &settings,
        SessionService &sessionController,
        ScriptService &scriptController,
        ApplicationSessionRuntime &sessionRuntime);

    bool loadSessions(QString &errorMessage);
    bool saveSessions(QString &errorMessage);

private:
    QSettings &m_settings;
    SessionService &m_sessionController;
    ScriptService &m_scriptController;
    ApplicationSessionRuntime &m_sessionRuntime;
};
