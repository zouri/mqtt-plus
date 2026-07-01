#include "app/applicationmodelrefresher.h"

#include "services/apputils.h"
#include "controllers/scriptcontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/subscriptioncontroller.h"
#include "domain/sessionconfig.h"
#include "models/scriptlibrarymodel.h"
#include "models/scripttestsamplesmodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionlistmodel.h"
#include "services/payload/payloadcodec.h"
#include "services/storage/scriptstore.h"

#include <QDateTime>

using namespace AppUtils;

ApplicationModelRefresher::ApplicationModelRefresher(
    SessionController &sessionController,
    ScriptController &scriptController,
    SubscriptionController &subscriptionController,
    SessionListModel &sessionsModel,
    SubscriptionListModel &subscriptionsModel,
    ScriptLibraryModel &scriptsModel,
    ScriptTestSamplesModel &scriptTestSamplesModel)
    : m_sessionController(sessionController)
    , m_scriptController(scriptController)
    , m_subscriptionController(subscriptionController)
    , m_sessionsModel(sessionsModel)
    , m_subscriptionsModel(subscriptionsModel)
    , m_scriptsModel(scriptsModel)
    , m_scriptTestSamplesModel(scriptTestSamplesModel)
{
}

void ApplicationModelRefresher::refreshSessions()
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

void ApplicationModelRefresher::refreshSubscriptions(const SessionState *currentSession)
{
    if (!currentSession) {
        m_subscriptionsModel.setRows({});
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QVector<SubscriptionListRow> rows;
    rows.reserve(currentSession->subscriptions.size());
    for (const auto &subscription : currentSession->subscriptions) {
        SubscriptionListRow row;
        row.topic = subscription.topic;
        row.alias = subscription.alias;
        row.displayName = subscription.alias.isEmpty() ? subscription.topic : subscription.alias;
        row.requestedQos = subscription.requestedQos;
        row.grantedQos = subscription.grantedQos;
        row.topicFps = m_subscriptionController.subscriptionFps(subscription, nowMs);
        row.format = subscription.format;
        row.formatName = PayloadCodec::formatName(PayloadCodec::formatFromInt(subscription.format));
        row.scriptId = subscription.scriptId;
        row.scriptName = m_scriptController.scriptName(subscription.scriptId);
        row.paused = subscription.paused;
        row.state = subscriptionDisplayState(*currentSession, subscription, currentSession->client);
        row.lastError = subscription.lastError;
        rows.append(row);
    }
    m_subscriptionsModel.setRows(rows);
}

void ApplicationModelRefresher::refreshScripts()
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

void ApplicationModelRefresher::refreshScriptTestSamples(const SessionState *currentSession)
{
    if (!currentSession) {
        m_scriptTestSamplesModel.setRows({});
        return;
    }

    constexpr int kMaxScriptTestSamples = 24;
    QVector<ScriptTestSampleRow> rows;
    rows.reserve(kMaxScriptTestSamples);

    for (auto it = currentSession->messageRows.crbegin();
         it != currentSession->messageRows.crend() && rows.size() < kMaxScriptTestSamples;
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
