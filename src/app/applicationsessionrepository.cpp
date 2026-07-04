#include "app/applicationsessionrepository.h"

#include "app/applicationsessionconfigurator.h"
#include "app/applicationsessionruntime.h"
#include "usecases/scriptservice.h"
#include "usecases/sessionservice.h"
#include "services/storage/sessionsettingsstore.h"

#include <QCoreApplication>
#include <QSettings>

ApplicationSessionRepository::ApplicationSessionRepository(
    QSettings &settings,
    SessionService &sessionController,
    ScriptService &scriptController,
    ApplicationSessionRuntime &sessionRuntime)
    : m_settings(settings)
    , m_sessionController(sessionController)
    , m_scriptController(scriptController)
    , m_sessionRuntime(sessionRuntime)
{
}

bool ApplicationSessionRepository::loadSessions(QString &errorMessage)
{
    errorMessage.clear();

    const int count = m_settings.beginReadArray(QStringLiteral("sessions"));
    for (int i = 0; i < count; ++i) {
        SessionSettingsStore::LoadedSession loaded = SessionSettingsStore::readSession(
            m_settings,
            i,
            [this](const QString &scriptId) { return m_scriptController.scriptById(scriptId) != nullptr; });
        SessionState session = loaded.session;

        m_sessionRuntime.initialize(&session);
        ApplicationSessionConfigurator::applyConfig(session, loaded.config, false);
        m_sessionRuntime.bindSignals(&session);
        m_sessionController.appendSession(session);
    }
    m_settings.endArray();

    if (!m_sessionController.sessions().isEmpty()) {
        return true;
    }

    m_sessionController.appendSession(m_sessionRuntime.createDefaultSession(
        QCoreApplication::translate("ApplicationSessionRepository", "Session 1")));
    return saveSessions(errorMessage);
}

bool ApplicationSessionRepository::saveSessions(QString &errorMessage)
{
    return SessionSettingsStore::writeSessions(m_settings, m_sessionController.sessions(), errorMessage);
}
