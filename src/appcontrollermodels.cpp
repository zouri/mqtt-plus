#include "appcontroller.h"

#include "appcontrollerutils.h"
#include "scriptstore.h"
#include "sessionconfig.h"

#include <QDateTime>

using namespace AppControllerUtils;

QVariantList AppController::sessionsModel() const
{
    QVariantList rows;
    rows.reserve(m_sessions.size());
    for (const auto &session : m_sessions) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), session.id);
        row.insert(QStringLiteral("name"), session.name);
        row.insert(QStringLiteral("state"), sessionStateName(session));
        row.insert(QStringLiteral("connected"), session.client && session.client->state() == QMqttClient::Connected);
        row.insert(QStringLiteral("host"), session.client ? session.client->hostname() : QString());
        row.insert(QStringLiteral("port"), session.client ? session.client->port() : SessionConfig::kDefaultPort);
        row.insert(QStringLiteral("transport"), session.transport);
        row.insert(QStringLiteral("transportLabel"), transportLabel(session.transport));
        row.insert(QStringLiteral("protocolVersion"), session.protocolVersion);
        row.insert(QStringLiteral("protocolVersionName"), protocolVersionLabel(session.protocolVersion));
        row.insert(QStringLiteral("summary"), session.brokerInfo.isEmpty() ? session.lastError : session.brokerInfo);
        row.insert(QStringLiteral("lastError"), session.lastError);
        rows.append(row);
    }
    return rows;
}

int AppController::currentSessionIndex() const
{
    return m_currentSessionIndex;
}

QVariantMap AppController::currentSession() const
{
    const auto *session = currentSessionState();
    if (!session) {
        return {};
    }

    QVariantMap row;
    row.insert(QStringLiteral("id"), session->id);
    row.insert(QStringLiteral("name"), session->name);
    row.insert(QStringLiteral("host"), session->client ? session->client->hostname() : QString());
    row.insert(QStringLiteral("port"), session->client ? session->client->port() : SessionConfig::kDefaultPort);
    row.insert(QStringLiteral("transport"), session->transport);
    row.insert(QStringLiteral("transportLabel"), transportLabel(session->transport));
    row.insert(QStringLiteral("protocolVersion"), session->protocolVersion);
    row.insert(QStringLiteral("protocolVersionName"), protocolVersionLabel(session->protocolVersion));
    row.insert(QStringLiteral("clientId"), session->client ? session->client->clientId() : QString());
    row.insert(QStringLiteral("username"), session->client ? session->client->username() : QString());
    row.insert(QStringLiteral("cleanSession"), session->client ? session->client->cleanSession() : true);
    row.insert(QStringLiteral("keepAliveSeconds"), session->client ? session->client->keepAlive() : SessionConfig::kDefaultKeepAlive);
    row.insert(QStringLiteral("outputPaused"), session->outputPaused);
    row.insert(QStringLiteral("subscriptionCount"), session->subscriptions.size());
    return row;
}

QVariantMap AppController::sessionStatus() const
{
    const auto *session = currentSessionState();
    if (!session) {
        return {};
    }

    const QString state = sessionStateName(*session);
    QString summary;
    if (state == QStringLiteral("connected")) {
        summary = QStringLiteral("%1 • %2:%3 • %4")
                      .arg(protocolVersionLabel(session->protocolVersion))
                      .arg(session->client->hostname())
                      .arg(session->client->port())
                      .arg(transportLabel(session->transport));
        if (session->sessionRestored) {
            summary.append(QStringLiteral(" • session restored"));
        }
    } else if (state == QStringLiteral("connecting")) {
        summary = QStringLiteral("Connecting to %1:%2 over %3")
                      .arg(session->client->hostname())
                      .arg(session->client->port())
                      .arg(transportLabel(session->transport));
    } else if (state == QStringLiteral("disconnecting")) {
        summary = QStringLiteral("Disconnecting from broker");
    } else if (!session->lastError.isEmpty()) {
        summary = session->lastError;
    } else {
        summary = QStringLiteral("Disconnected");
    }

    QVariantMap row;
    row.insert(QStringLiteral("state"), state);
    row.insert(QStringLiteral("connected"), state == QStringLiteral("connected"));
    row.insert(QStringLiteral("summary"), summary);
    row.insert(QStringLiteral("brokerInfo"), session->brokerInfo);
    row.insert(QStringLiteral("sessionRestored"), session->sessionRestored);
    row.insert(QStringLiteral("transportLabel"), transportLabel(session->transport));
    row.insert(QStringLiteral("protocolVersionName"), protocolVersionLabel(session->protocolVersion));
    return row;
}

QVariantList AppController::subscriptionsModel() const
{
    const auto *session = currentSessionState();
    if (!session) {
        return {};
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QVariantList rows;
    rows.reserve(session->subscriptions.size());
    for (const auto &subscription : session->subscriptions) {
        QVariantMap row;
        row.insert(QStringLiteral("topic"), subscription.topic);
        row.insert(QStringLiteral("alias"), subscription.alias);
        row.insert(
            QStringLiteral("displayName"),
            subscription.alias.isEmpty() ? subscription.topic : subscription.alias);
        row.insert(QStringLiteral("requestedQos"), subscription.requestedQos);
        row.insert(QStringLiteral("grantedQos"), subscription.grantedQos);
        row.insert(QStringLiteral("topicFps"), subscriptionFps(subscription, nowMs));
        row.insert(QStringLiteral("format"), subscription.format);
        row.insert(QStringLiteral("formatName"), PayloadCodec::formatName(PayloadCodec::formatFromInt(subscription.format)));
        row.insert(QStringLiteral("scriptId"), subscription.scriptId);
        row.insert(QStringLiteral("scriptName"), scriptName(subscription.scriptId));
        row.insert(QStringLiteral("paused"), subscription.paused);
        row.insert(QStringLiteral("state"), subscriptionDisplayState(*session, subscription));
        row.insert(QStringLiteral("lastError"), subscription.lastError);
        rows.append(row);
    }
    return rows;
}

QVariantMap AppController::publishStatus() const
{
    const auto *session = currentSessionState();
    return session ? session->publishStatus : defaultPublishStatus();
}

QVariantList AppController::eventStreamModel() const
{
    const auto *session = currentSessionState();
    return session ? session->eventRows : QVariantList {};
}

QStringList AppController::payloadFormats() const
{
    return PayloadCodec::formatNames();
}

QVariantList AppController::scriptLibraryModel() const
{
    QVariantList rows;
    rows.reserve(m_scripts.size());
    for (const auto &script : m_scripts) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), script.id);
        row.insert(QStringLiteral("name"), script.name);
        row.insert(QStringLiteral("code"), script.code);
        row.insert(QStringLiteral("updatedAt"), script.updatedAt);
        row.insert(QStringLiteral("filePath"), ScriptStore::scriptFilePath(script.fileName));
        rows.append(row);
    }
    return rows;
}

QVariantList AppController::scriptTestSamplesModel() const
{
    const auto *session = currentSessionState();
    if (!session) {
        return {};
    }

    constexpr int kMaxScriptTestSamples = 24;
    QVariantList rows;
    rows.reserve(kMaxScriptTestSamples);

    for (auto it = session->eventRows.crbegin();
         it != session->eventRows.crend() && rows.size() < kMaxScriptTestSamples;
         ++it) {
        const QVariantMap row = it->toMap();
        if (row.value(QStringLiteral("kind")).toString() != QStringLiteral("message")) {
            continue;
        }

        QVariantMap sample;
        sample.insert(QStringLiteral("topic"), row.value(QStringLiteral("topic")).toString());
        sample.insert(QStringLiteral("payload"), row.value(QStringLiteral("testPayload")).toString());
        sample.insert(QStringLiteral("format"), row.value(QStringLiteral("testFormat")).toInt());
        sample.insert(QStringLiteral("formatName"), row.value(QStringLiteral("testFormatName")).toString());
        sample.insert(QStringLiteral("timestamp"), row.value(QStringLiteral("timestamp")).toString());
        sample.insert(QStringLiteral("payloadSize"), row.value(QStringLiteral("payloadSize")).toInt());
        rows.append(sample);
    }

    return rows;
}

QString AppController::themeMode() const
{
    return m_themeMode;
}

QString AppController::effectiveTheme() const
{
    if (m_themeMode == QStringLiteral("light") || m_themeMode == QStringLiteral("dark")) {
        return m_themeMode;
    }
    return m_systemDarkMode ? QStringLiteral("dark") : QStringLiteral("light");
}

