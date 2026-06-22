#include "app/appfacade.h"

#include "app/appfacadeutils.h"
#include "domain/sessionconfig.h"
#include "services/storage/scriptstore.h"

#include <QDateTime>

using namespace AppFacadeUtils;

WorkbenchFacade *AppFacade::workbench()
{
    return m_workbenchFacade.get();
}

AppSettingsFacade *AppFacade::settings()
{
    return m_settingsFacade.get();
}

ScriptLibraryFacade *AppFacade::scriptLibrary()
{
    return m_scriptLibraryFacade.get();
}

LogStreamFacade *AppFacade::logStream()
{
    return m_logStreamFacade.get();
}

void AppFacade::refreshSessionsModel()
{
    QVector<SessionListRow> rows;
    const auto &sessions = m_sessionController.sessions();
    rows.reserve(sessions.size());
    for (const auto &session : sessions) {
        const auto *client = session.client;
        SessionListRow row;
        row.id = session.id;
        row.name = session.name;
        row.state = sessionStateName(session, client);
        row.connected = client && client->state() == QMqttClient::Connected;
        row.host = client ? client->hostname() : QString();
        row.port = client ? client->port() : SessionConfig::kDefaultPort;
        row.transport = session.transport;
        row.transportLabel = transportLabel(session.transport);
        row.protocolVersion = session.protocolVersion;
        row.protocolVersionName = protocolVersionLabel(session.protocolVersion);
        row.summary = session.brokerInfo.isEmpty() ? session.lastError : session.brokerInfo;
        row.lastError = session.lastError;
        rows.append(row);
    }
    m_sessionsModel.setRows(rows);
}

void AppFacade::refreshSubscriptionsModel()
{
    const auto *session = currentSessionState();
    if (!session) {
        m_subscriptionsModel.setRows({});
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QVector<SubscriptionListRow> rows;
    rows.reserve(session->subscriptions.size());
    for (const auto &subscription : session->subscriptions) {
        SubscriptionListRow row;
        row.topic = subscription.topic;
        row.alias = subscription.alias;
        row.displayName = subscription.alias.isEmpty() ? subscription.topic : subscription.alias;
        row.requestedQos = subscription.requestedQos;
        row.grantedQos = subscription.grantedQos;
        row.topicFps = subscriptionFps(subscription, nowMs);
        row.format = subscription.format;
        row.formatName = PayloadCodec::formatName(PayloadCodec::formatFromInt(subscription.format));
        row.scriptId = subscription.scriptId;
        row.scriptName = scriptName(subscription.scriptId);
        row.paused = subscription.paused;
        row.state = subscriptionDisplayState(*session, subscription, session->client);
        row.lastError = subscription.lastError;
        rows.append(row);
    }
    m_subscriptionsModel.setRows(rows);
}

void AppFacade::refreshScriptsModel()
{
    QVector<ScriptLibraryRow> rows;
    const auto &scripts = m_scriptController.scripts();
    rows.reserve(scripts.size());
    for (const auto &script : scripts) {
        ScriptLibraryRow row;
        row.id = script.id;
        row.name = script.name;
        row.description = script.description;
        row.code = script.code;
        row.updatedAt = displayTimestamp(script.updatedAt);
        row.filePath = ScriptStore::scriptFilePath(script.fileName);
        rows.append(row);
    }
    m_scriptsModel.setRows(rows);
}

void AppFacade::refreshScriptTestSamplesModel()
{
    const auto *session = currentSessionState();
    if (!session) {
        m_scriptTestSamplesModel.setRows({});
        return;
    }

    constexpr int kMaxScriptTestSamples = 24;
    QVector<ScriptTestSampleRow> rows;
    rows.reserve(kMaxScriptTestSamples);

    for (auto it = session->messageRows.crbegin();
         it != session->messageRows.crend() && rows.size() < kMaxScriptTestSamples;
         ++it) {
        const QVariantMap row = it->toMap();
        ScriptTestSampleRow sample;
        sample.topic = row.value(QStringLiteral("topic")).toString();
        sample.payload = row.value(QStringLiteral("testPayload")).toString();
        sample.format = row.value(QStringLiteral("testFormat")).toInt();
        sample.formatName = row.value(QStringLiteral("testFormatName")).toString();
        sample.timestamp = row.value(QStringLiteral("timestamp")).toString();
        sample.payloadSize = row.value(QStringLiteral("payloadSize")).toInt();
        rows.append(sample);
    }

    m_scriptTestSamplesModel.setRows(rows);
}
