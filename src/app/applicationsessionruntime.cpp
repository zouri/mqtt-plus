#include "app/applicationsessionruntime.h"

#include "services/apputils.h"
#include "app/applicationsessionconfigurator.h"
#include "domain/sessionconfig.h"

#include <QCoreApplication>
#include <QMqttClient>
#include <QTimer>
#include <QUuid>

using namespace AppUtils;

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

    if (!session->client) {
        session->client = new QMqttClient(m_owner);
        session->client->setAutoKeepAlive(true);
    }
    if (!session->connectTimeoutTimer) {
        session->connectTimeoutTimer = new QTimer(m_owner);
        session->connectTimeoutTimer->setSingleShot(true);
        QObject::connect(session->connectTimeoutTimer, &QTimer::timeout, m_owner, [this, sessionId = session->id]() {
            auto *boundSession = m_callbacks.sessionById ? m_callbacks.sessionById(sessionId) : nullptr;
            auto *client = boundSession ? boundSession->client : nullptr;
            if (!boundSession || !client || client->state() != QMqttClient::Connecting) {
                return;
            }

            const QString timeoutMessage = QCoreApplication::translate("ApplicationSessionRuntime", "Connection timed out.");
            boundSession->lastError = timeoutMessage;
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
    if (session.connectTimeoutTimer) {
        session.connectTimeoutTimer->stop();
        session.connectTimeoutTimer->deleteLater();
        session.connectTimeoutTimer = nullptr;
    }
    if (session.client) {
        session.client->disconnectFromHost();
        session.client->deleteLater();
        session.client = nullptr;
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
    session.publishStatus = defaultPublishStatus();
    initialize(&session);
    QVariantMap config = SessionConfig::defaultConfig(1);
    config.insert(QStringLiteral("name"), name);
    ApplicationSessionConfigurator::applyConfig(session, config, false);
    bindSignals(&session);
    return session;
}
