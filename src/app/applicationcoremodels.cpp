#include "app/applicationcore.h"

#include "app/applicationcoreutils.h"
#include "domain/sessionconfig.h"
#include "services/storage/scriptstore.h"

#include <QDateTime>

using namespace ApplicationCoreUtils;

SessionListModel *ApplicationCore::sessions()
{
    return &m_sessionsModel;
}

SubscriptionListModel *ApplicationCore::subscriptions()
{
    return &m_subscriptionsModel;
}

SubscriptionFilterModel *ApplicationCore::filteredSubscriptions()
{
    return &m_filteredSubscriptionsModel;
}

EventStreamModel *ApplicationCore::messages()
{
    return &m_messagesModel;
}

EventStreamModel *ApplicationCore::logs()
{
    return &m_logsModel;
}

ScriptLibraryModel *ApplicationCore::scripts()
{
    return &m_scriptsModel;
}

ScriptTestSamplesModel *ApplicationCore::scriptTestSamples()
{
    return &m_scriptTestSamplesModel;
}

void ApplicationCore::refreshSessionsModel()
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

int ApplicationCore::currentSessionIndex() const
{
    return m_sessionController.currentIndex();
}

QVariantMap ApplicationCore::currentSession() const
{
    const auto *session = currentSessionState();
    if (!session) {
        return {};
    }

    QVariantMap row;
    const auto *client = session->client;
    row.insert(QStringLiteral("id"), session->id);
    row.insert(QStringLiteral("name"), session->name);
    row.insert(QStringLiteral("host"), client ? client->hostname() : QString());
    row.insert(QStringLiteral("port"), client ? client->port() : SessionConfig::kDefaultPort);
    row.insert(QStringLiteral("transport"), session->transport);
    row.insert(QStringLiteral("transportLabel"), transportLabel(session->transport));
    row.insert(QStringLiteral("protocolVersion"), session->protocolVersion);
    row.insert(QStringLiteral("protocolVersionName"), protocolVersionLabel(session->protocolVersion));
    row.insert(QStringLiteral("clientId"), client ? client->clientId() : QString());
    row.insert(QStringLiteral("username"), client ? client->username() : QString());
    row.insert(QStringLiteral("cleanSession"), client ? client->cleanSession() : true);
    row.insert(QStringLiteral("keepAliveSeconds"), client ? client->keepAlive() : SessionConfig::kDefaultKeepAlive);
    row.insert(QStringLiteral("outputPaused"), session->outputPaused);
    row.insert(QStringLiteral("subscriptionCount"), session->subscriptions.size());
    return row;
}

QVariantMap ApplicationCore::sessionStatus() const
{
    const auto *session = currentSessionState();
    if (!session) {
        return {};
    }

    const auto *client = session->client;
    const QString state = sessionStateName(*session, client);
    QString summary;
    if (state == QStringLiteral("connected")) {
        summary = tr("%1 • %2:%3 • %4")
                      .arg(protocolVersionLabel(session->protocolVersion))
                      .arg(client ? client->hostname() : QString())
                      .arg(client ? client->port() : SessionConfig::kDefaultPort)
                      .arg(transportLabel(session->transport));
        if (session->sessionRestored) {
            summary.append(tr(" • session restored"));
        }
    } else if (state == QStringLiteral("connecting")) {
        summary = tr("Connecting to %1:%2 over %3")
                      .arg(client ? client->hostname() : QString())
                      .arg(client ? client->port() : SessionConfig::kDefaultPort)
                      .arg(transportLabel(session->transport));
    } else if (state == QStringLiteral("disconnecting")) {
        summary = tr("Disconnecting from broker");
    } else if (!session->lastError.isEmpty()) {
        summary = session->lastError;
    } else {
        summary = tr("Disconnected");
    }

    QVariantMap row;
    row.insert(QStringLiteral("state"), state);
    row.insert(QStringLiteral("connected"), state == QStringLiteral("connected"));
    row.insert(QStringLiteral("summary"), summary);
    row.insert(QStringLiteral("lastError"), session->lastError);
    row.insert(QStringLiteral("hasError"), !session->lastError.isEmpty());
    row.insert(QStringLiteral("brokerInfo"), session->brokerInfo);
    row.insert(QStringLiteral("sessionRestored"), session->sessionRestored);
    row.insert(QStringLiteral("transportLabel"), transportLabel(session->transport));
    row.insert(QStringLiteral("protocolVersionName"), protocolVersionLabel(session->protocolVersion));
    return row;
}

void ApplicationCore::refreshSubscriptionsModel()
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

QVariantMap ApplicationCore::publishStatus() const
{
    const auto *session = currentSessionState();
    QVariantMap status = session ? session->publishStatus : defaultPublishStatus();
    status.insert(
        QStringLiteral("updatedAt"),
        displayTimestamp(status.value(QStringLiteral("updatedAt")).toString()));
    return status;
}

QStringList ApplicationCore::payloadFormats() const
{
    return PayloadCodec::formatNames();
}

void ApplicationCore::refreshScriptsModel()
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

void ApplicationCore::refreshScriptTestSamplesModel()
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

QString ApplicationCore::themeMode() const
{
    return m_themeController.mode();
}

QString ApplicationCore::effectiveTheme() const
{
    return m_themeController.effectiveTheme();
}

QString ApplicationCore::languageMode() const
{
    return m_languageController.mode();
}

QString ApplicationCore::effectiveLanguage() const
{
    return m_languageController.effectiveLanguage();
}

QVariantList ApplicationCore::availableLanguages() const
{
    return m_languageController.availableLanguages();
}
