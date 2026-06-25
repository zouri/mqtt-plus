#include "app/modelcoordinator.h"

#include "app/appruntimeutils.h"
#include "controllers/scriptcontroller.h"
#include "controllers/sessioncontroller.h"
#include "controllers/subscriptioncontroller.h"
#include "domain/session.h"
#include "domain/sessionconfig.h"
#include "models/eventstreammodel.h"
#include "models/scriptlibrarymodel.h"
#include "models/scripttestsamplesmodel.h"
#include "models/sessionlistmodel.h"
#include "models/subscriptionlistmodel.h"
#include "services/payload/payloadcodec.h"
#include "services/storage/scriptstore.h"

#include <QDateTime>

#include <utility>

using namespace AppRuntimeUtils;

ModelCoordinator::ModelCoordinator(Dependencies dependencies)
    : m_dependencies(std::move(dependencies))
{
}

void ModelCoordinator::refreshSessionsModel()
{
    if (!m_dependencies.sessionController || !m_dependencies.sessionsModel) {
        return;
    }

    QVector<SessionListRow> rows;
    const auto &sessions = m_dependencies.sessionController->sessions();
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
    m_dependencies.sessionsModel->setRows(rows);
}

void ModelCoordinator::refreshSubscriptionsModel()
{
    if (!m_dependencies.subscriptionsModel) {
        return;
    }

    const auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session) {
        m_dependencies.subscriptionsModel->setRows({});
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
        row.topicFps = m_dependencies.subscriptionController
            ? m_dependencies.subscriptionController->subscriptionFps(subscription, nowMs)
            : 0.0;
        row.format = subscription.format;
        row.formatName = PayloadCodec::formatName(PayloadCodec::formatFromInt(subscription.format));
        row.scriptId = subscription.scriptId;
        row.scriptName = m_dependencies.scriptController
            ? m_dependencies.scriptController->scriptName(subscription.scriptId)
            : QString();
        row.paused = subscription.paused;
        row.state = subscriptionDisplayState(*session, subscription, session->client);
        row.lastError = subscription.lastError;
        rows.append(row);
    }
    m_dependencies.subscriptionsModel->setRows(rows);
}

void ModelCoordinator::refreshScriptsModel()
{
    if (!m_dependencies.scriptController || !m_dependencies.scriptsModel) {
        return;
    }

    QVector<ScriptLibraryRow> rows;
    const auto &scripts = m_dependencies.scriptController->scripts();
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
    m_dependencies.scriptsModel->setRows(rows);
}

void ModelCoordinator::refreshScriptTestSamplesModel()
{
    if (!m_dependencies.scriptTestSamplesModel) {
        return;
    }

    const auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (!session) {
        m_dependencies.scriptTestSamplesModel->setRows({});
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

    m_dependencies.scriptTestSamplesModel->setRows(rows);
}

void ModelCoordinator::syncSelectedSessionModels()
{
    refreshSubscriptionsModel();
    syncCurrentSessionEventModels();
    refreshScriptTestSamplesModel();
}

void ModelCoordinator::syncSessionCollectionModels()
{
    refreshSessionsModel();
    refreshSubscriptionsModel();
    syncCurrentSessionEventModels();
    refreshScriptsModel();
    refreshScriptTestSamplesModel();
}

void ModelCoordinator::syncCurrentSessionEventModels()
{
    const auto *session = m_dependencies.currentSession ? m_dependencies.currentSession() : nullptr;
    if (m_dependencies.messagesModel) {
        m_dependencies.messagesModel->setRows(session ? session->messageRows : QVariantList {});
    }
    if (m_dependencies.logsModel) {
        m_dependencies.logsModel->setRows(session ? session->logRows : QVariantList {});
    }
}
