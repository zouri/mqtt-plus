#include "app/applicationsessionruntime.h"

#include "app/applicationsessionconfigurator.h"
#include "domain/sessionconfig.h"

#include <QCoreApplication>
#include <QMqttClient>
#include <QTimer>
#include <QUuid>

ApplicationSessionRuntime::ApplicationSessionRuntime(QObject *owner, ApplicationSessionRuntimeCallbacks callbacks)
    : m_owner(owner)
    , m_callbacks(std::move(callbacks))
{
}

void ApplicationSessionRuntime::initialize(SessionState *session)
{
    if (!session || !m_owner) {
        return;
    }

    if (!session->runtime.client) {
        session->runtime.client = new QMqttClient(m_owner);
        session->runtime.client->setAutoKeepAlive(true);
    }
    if (!session->runtime.connectTimeoutTimer) {
        session->runtime.connectTimeoutTimer = new QTimer(m_owner);
        session->runtime.connectTimeoutTimer->setSingleShot(true);
        QObject::connect(session->runtime.connectTimeoutTimer, &QTimer::timeout, m_owner, [this, sessionId = session->id]() {
            auto *boundSession = m_callbacks.sessionById ? m_callbacks.sessionById(sessionId) : nullptr;
            auto *client = boundSession ? boundSession->runtime.client : nullptr;
            if (!boundSession || !client || client->state() != QMqttClient::Connecting) {
                return;
            }

            const QString timeoutMessage = QCoreApplication::translate("ApplicationSessionRuntime", "Connection timed out.");
            boundSession->runtime.lastError = timeoutMessage;
            if (m_callbacks.appendEvent) {
                m_callbacks.appendEvent(*boundSession, QStringLiteral("Error"), timeoutMessage);
            }
            client->disconnectFromHost();
            if (m_callbacks.notifySessionViewsChanged) {
                m_callbacks.notifySessionViewsChanged();
            }
        });
    }
}

void ApplicationSessionRuntime::destroy(SessionState &session)
{
    if (session.runtime.connectTimeoutTimer) {
        session.runtime.connectTimeoutTimer->stop();
        session.runtime.connectTimeoutTimer->deleteLater();
        session.runtime.connectTimeoutTimer = nullptr;
    }
    if (session.runtime.client) {
        session.runtime.client->disconnectFromHost();
        session.runtime.client->deleteLater();
        session.runtime.client = nullptr;
    }
}

void ApplicationSessionRuntime::bindSignals(SessionState *session)
{
    if (m_callbacks.bindSessionSignals) {
        m_callbacks.bindSessionSignals(session);
    }
}

SessionState ApplicationSessionRuntime::createDefaultSession(const QString &name)
{
    SessionState session;
    session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.name = name;
    session.transport = QStringLiteral("tcp");
    session.protocolVersion = 5;
    initialize(&session);
    QVariantMap config = SessionConfig::defaultConfig(1);
    config.insert(QStringLiteral("name"), name);
    ApplicationSessionConfigurator::applyConfig(session, config, false);
    bindSignals(&session);
    return session;
}
