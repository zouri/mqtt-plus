#include "appcontroller.h"

#include "eventrenderer.h"
#include "scriptstore.h"
#include "sessionconfig.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QFile>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QStyleHints>
#include <QUuid>

#include <algorithm>

namespace {
constexpr int kEventPageSize = 500;
constexpr int kMaxVisibleEventRows = 1200;
constexpr qint64 kSubscriptionFpsWindowMs = 1000;
constexpr int kSubscriptionFpsRefreshIntervalMs = 250;

QString timestampNow()
{
    return QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
}

QString transportLabel(const QString &transport)
{
    return transport == QStringLiteral("tls") ? QStringLiteral("TLS") : QStringLiteral("TCP");
}

QString protocolVersionLabel(int protocolVersion)
{
    return protocolVersion >= 5 ? QStringLiteral("MQTT 5") : QStringLiteral("MQTT 3.1.1");
}

QList<QByteArray> alpnProtocols(const QString &alpn)
{
    QList<QByteArray> protocols;
    const QStringList parts = alpn.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        protocols.append(part.trimmed().toUtf8());
    }
    return protocols;
}

QSslKey readPrivateKey(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QByteArray keyData = file.readAll();
    for (const QSsl::KeyAlgorithm algorithm : {QSsl::Rsa, QSsl::Dsa, QSsl::Ec}) {
        QSslKey key(keyData, algorithm);
        if (!key.isNull()) {
            return key;
        }
    }
    return {};
}

QString sanitizeThemeMode(const QString &value)
{
    const QString mode = value.trimmed().toLower();
    if (mode == QStringLiteral("light") || mode == QStringLiteral("dark")) {
        return mode;
    }
    return QStringLiteral("system");
}

QMqttClient::ProtocolVersion toProtocolVersion(int value)
{
    return value >= 5 ? QMqttClient::MQTT_5_0 : QMqttClient::MQTT_3_1_1;
}

QString subscriptionStateName(QMqttSubscription::SubscriptionState state)
{
    switch (state) {
    case QMqttSubscription::Unsubscribed:
        return QStringLiteral("unsubscribed");
    case QMqttSubscription::SubscriptionPending:
        return QStringLiteral("pending");
    case QMqttSubscription::Subscribed:
        return QStringLiteral("subscribed");
    case QMqttSubscription::UnsubscriptionPending:
        return QStringLiteral("unsubscribing");
    case QMqttSubscription::Error:
        return QStringLiteral("error");
    }
    return QStringLiteral("saved");
}

QString clientErrorName(QMqttClient::ClientError error)
{
    switch (error) {
    case QMqttClient::NoError:
        return QStringLiteral("No error");
    case QMqttClient::InvalidProtocolVersion:
        return QStringLiteral("Protocol version rejected by broker");
    case QMqttClient::IdRejected:
        return QStringLiteral("Client ID rejected");
    case QMqttClient::ServerUnavailable:
        return QStringLiteral("Broker unavailable");
    case QMqttClient::BadUsernameOrPassword:
        return QStringLiteral("Username or password rejected");
    case QMqttClient::NotAuthorized:
        return QStringLiteral("Not authorized");
    case QMqttClient::TransportInvalid:
        return QStringLiteral("Invalid transport");
    case QMqttClient::ProtocolViolation:
        return QStringLiteral("Protocol violation");
    case QMqttClient::UnknownError:
        return QStringLiteral("Unknown MQTT error");
    case QMqttClient::Mqtt5SpecificError:
        return QStringLiteral("MQTT 5 broker reported an error");
    }
    return QStringLiteral("MQTT error");
}

QString messageStatusName(QMqtt::MessageStatus status)
{
    switch (status) {
    case QMqtt::MessageStatus::Unknown:
        return QStringLiteral("queued");
    case QMqtt::MessageStatus::Published:
        return QStringLiteral("published");
    case QMqtt::MessageStatus::Acknowledged:
        return QStringLiteral("acknowledged");
    case QMqtt::MessageStatus::Received:
        return QStringLiteral("received");
    case QMqtt::MessageStatus::Released:
        return QStringLiteral("released");
    case QMqtt::MessageStatus::Completed:
        return QStringLiteral("completed");
    }
    return QStringLiteral("queued");
}

QString socketDiagnostic(QMqttClient *client)
{
    if (!client || !client->transport()) {
        return QString();
    }

    const auto *socket = qobject_cast<QAbstractSocket *>(client->transport());
    if (!socket) {
        return QString();
    }
    return socket->errorString();
}

int topicSpecificityScore(const QString &filter)
{
    int score = filter.count('/');
    for (const QChar character : filter) {
        if (character != QLatin1Char('#')
                && character != QLatin1Char('+')
                && character != QLatin1Char('/')) {
            ++score;
        }
    }
    return score;
}

QString subscriptionDisplayState(const AppController::SessionState &session, const AppController::SubscriptionEntry &entry)
{
    if (entry.paused) {
        return QStringLiteral("paused");
    }
    if (!session.client || session.client->state() != QMqttClient::Connected) {
        return QStringLiteral("saved");
    }
    return entry.runtimeState.isEmpty() ? QStringLiteral("saved") : entry.runtimeState;
}

QString sessionStateName(const AppController::SessionState &session)
{
    if (session.disconnectRequested && session.client
        && session.client->state() != QMqttClient::Disconnected) {
        return QStringLiteral("disconnecting");
    }

    if (!session.client) {
        return QStringLiteral("disconnected");
    }

    switch (session.client->state()) {
    case QMqttClient::Disconnected:
        return QStringLiteral("disconnected");
    case QMqttClient::Connecting:
        return QStringLiteral("connecting");
    case QMqttClient::Connected:
        return QStringLiteral("connected");
    }
    return QStringLiteral("disconnected");
}

QVariantMap defaultPublishStatus()
{
    QVariantMap status;
    status.insert(QStringLiteral("state"), QStringLiteral("idle"));
    status.insert(QStringLiteral("topic"), QString());
    status.insert(QStringLiteral("reason"), QString());
    status.insert(QStringLiteral("messageId"), -1);
    status.insert(QStringLiteral("qos"), 0);
    status.insert(QStringLiteral("retain"), false);
    status.insert(QStringLiteral("updatedAt"), QString());
    return status;
}

void pruneRecentMessageTimestamps(QVector<qint64> &timestamps, qint64 nowMs)
{
    const qint64 cutoffMs = nowMs - kSubscriptionFpsWindowMs;
    timestamps.erase(
        std::remove_if(
            timestamps.begin(),
            timestamps.end(),
            [cutoffMs](qint64 timestampMs) { return timestampMs < cutoffMs; }),
        timestamps.end());
}

int recentMessageCount(const QVector<qint64> &timestamps, qint64 nowMs)
{
    const qint64 cutoffMs = nowMs - kSubscriptionFpsWindowMs;
    return static_cast<int>(std::count_if(
        timestamps.cbegin(),
        timestamps.cend(),
        [cutoffMs](qint64 timestampMs) { return timestampMs >= cutoffMs; }));
}
}

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"))
{
    m_launchTimestamp = timestampNow();
    m_themeMode = sanitizeThemeMode(
        m_settings.value(QStringLiteral("appearance/themeMode"), QStringLiteral("system")).toString());
    refreshSystemColorScheme();

    if (QGuiApplication::styleHints()) {
        connect(
            QGuiApplication::styleHints(),
            &QStyleHints::colorSchemeChanged,
            this,
            [this](Qt::ColorScheme) { refreshSystemColorScheme(); });
    }

    m_subscriptionFpsRefreshTimer.setInterval(kSubscriptionFpsRefreshIntervalMs);
    connect(
        &m_subscriptionFpsRefreshTimer,
        &QTimer::timeout,
        this,
        &AppController::refreshSubscriptionFps);
    loadScripts();
    loadSessions();
}

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

void AppController::setCurrentSessionIndex(int index)
{
    if (index < 0 || index >= m_sessions.size() || index == m_currentSessionIndex) {
        return;
    }

    m_currentSessionIndex = index;
    reloadCurrentSessionHistory();
    if (currentSessionHasActiveSubscriptionFps(QDateTime::currentMSecsSinceEpoch())) {
        m_subscriptionFpsRefreshTimer.start();
    } else {
        m_subscriptionFpsRefreshTimer.stop();
    }
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit eventStreamChanged();
    emit scriptLibraryChanged();
    emit scriptTestSamplesChanged();
}

void AppController::setThemeMode(const QString &mode)
{
    const QString sanitized = sanitizeThemeMode(mode);
    if (sanitized == m_themeMode) {
        return;
    }

    const QString previousEffectiveTheme = effectiveTheme();
    m_themeMode = sanitized;
    m_settings.setValue(QStringLiteral("appearance/themeMode"), m_themeMode);
    m_settings.sync();

    emit themeModeChanged();
    if (effectiveTheme() != previousEffectiveTheme) {
        emit effectiveThemeChanged();
    }
}

QVariantMap AppController::defaultSessionConfig() const
{
    return SessionConfig::defaultConfig(m_sessions.size() + 1);
}

QVariantMap AppController::sessionConfigAt(int index) const
{
    if (index < 0 || index >= m_sessions.size()) {
        return defaultSessionConfig();
    }

    return SessionConfig::configFromSession(m_sessions.at(index));
}

bool AppController::updateSessionConfigAt(int index, const QVariantMap &config)
{
    if (index < 0 || index >= m_sessions.size()) {
        return false;
    }

    auto *session = &m_sessions[index];
    const bool reconnect = session->client->state() != QMqttClient::Disconnected;
    if (reconnect) {
        session->disconnectRequested = true;
        session->client->disconnectFromHost();
    }

    configureSession(*session, config, true);
    session->lastError.clear();
    session->sessionRestored = false;
    updatePublishStatus(*session, QStringLiteral("idle"));
    saveSessions();

    if (reconnect) {
        session->disconnectRequested = false;
        connectSession(*session, QStringLiteral("Connecting to"));
    }

    emit sessionsChanged();
    if (index == m_currentSessionIndex) {
        emit currentSessionChanged();
        emit subscriptionsChanged();
    }
    return true;
}

void AppController::addSessionWithConfig(const QVariantMap &config)
{
    const QString fallbackName = config.value(QStringLiteral("name")).toString().trimmed().isEmpty()
        ? QStringLiteral("Session %1").arg(m_sessions.size() + 1)
        : config.value(QStringLiteral("name")).toString().trimmed();

    SessionState session = createDefaultSession(fallbackName);
    configureSession(session, config, false);
    m_sessions.append(session);
    m_currentSessionIndex = m_sessions.size() - 1;
    reloadCurrentSessionHistory();
    saveSessions();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit eventStreamChanged();
    emit scriptLibraryChanged();
}

void AppController::duplicateSessionAt(int index)
{
    if (index < 0 || index >= m_sessions.size()) {
        return;
    }

    const auto &source = m_sessions.at(index);
    const QVariantMap config = SessionConfig::duplicateConfigFromSession(source);

    SessionState session = createDefaultSession(QStringLiteral("%1 Copy").arg(source.name));
    configureSession(session, config, false);
    session.outputPaused = source.outputPaused;
    session.subscriptionFormats = source.subscriptionFormats;
    session.subscriptions = source.subscriptions;
    for (auto &subscription : session.subscriptions) {
        subscription.runtimeSubscription.clear();
        subscription.runtimeState = QStringLiteral("saved");
        subscription.grantedQos = -1;
        subscription.lastError.clear();
        subscription.recentMessageTimestampsMs.clear();
    }

    m_sessions.append(session);
    m_currentSessionIndex = m_sessions.size() - 1;
    reloadCurrentSessionHistory();
    saveSessions();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit eventStreamChanged();
    emit scriptLibraryChanged();
}

void AppController::removeSessionAt(int index)
{
    if (m_sessions.size() <= 1 || index < 0 || index >= m_sessions.size()) {
        return;
    }

    SessionState removed = m_sessions.takeAt(index);
    if (removed.client) {
        removed.client->disconnectFromHost();
        removed.client->deleteLater();
    }
    if (removed.connectTimeoutTimer) {
        removed.connectTimeoutTimer->deleteLater();
    }

    if (m_currentSessionIndex >= m_sessions.size()) {
        m_currentSessionIndex = m_sessions.size() - 1;
    }
    if (m_currentSessionIndex > index) {
        --m_currentSessionIndex;
    }

    reloadCurrentSessionHistory();
    saveSessions();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit eventStreamChanged();
    emit scriptLibraryChanged();
}

void AppController::connectCurrentSession()
{
    auto *session = currentSessionState();
    if (!session || !session->client) {
        return;
    }

    if (session->client->hostname().trimmed().isEmpty()) {
        session->lastError = QStringLiteral("Broker host cannot be empty.");
        appendEvent(*session, QStringLiteral("Connection"), session->lastError);
        emit sessionsChanged();
        emit currentSessionChanged();
        return;
    }

    if (session->client->clientId().trimmed().isEmpty()) {
        session->lastError = QStringLiteral("Client ID cannot be empty.");
        appendEvent(*session, QStringLiteral("Connection"), session->lastError);
        emit sessionsChanged();
        emit currentSessionChanged();
        return;
    }

    session->disconnectRequested = false;
    session->sessionRestored = false;
    session->lastError.clear();
    updatePublishStatus(*session, QStringLiteral("idle"));
    connectSession(*session, QStringLiteral("Connecting to"));

    emit sessionsChanged();
    emit currentSessionChanged();
}

void AppController::disconnectCurrentSession()
{
    auto *session = currentSessionState();
    if (!session || !session->client) {
        return;
    }

    session->disconnectRequested = true;
    if (session->connectTimeoutTimer) {
        session->connectTimeoutTimer->stop();
    }
    session->client->disconnectFromHost();
    emit sessionsChanged();
    emit currentSessionChanged();
}

void AppController::setCurrentOutputPaused(bool paused)
{
    auto *session = currentSessionState();
    if (!session || session->outputPaused == paused) {
        return;
    }

    session->outputPaused = paused;
    saveSessions();
    if (!paused) {
        reloadCurrentSessionHistory();
        emit eventStreamChanged();
    }
    emit currentSessionChanged();
}

bool AppController::upsertCurrentSubscription(
    const QString &topic,
    int qos,
    int format,
    const QString &scriptId,
    const QString &alias)
{
    auto *session = currentSessionState();
    if (!session) {
        return false;
    }

    const QString filter = topic.trimmed();
    if (filter.isEmpty()) {
        return false;
    }

    const QMqttTopicFilter topicFilter(filter);
    if (!topicFilter.isValid()) {
        appendEvent(*session, QStringLiteral("Subscription"), QStringLiteral("Invalid topic filter: %1").arg(filter));
        return false;
    }

    SubscriptionEntry *entry = subscriptionByTopic(session, filter);
    const QString sanitizedScriptId = scriptById(scriptId) ? scriptId : QString();
    const QString displayAlias = alias.trimmed();
    if (!entry) {
        SubscriptionEntry subscription;
        subscription.topic = filter;
        subscription.alias = displayAlias;
        subscription.requestedQos = SessionConfig::sanitizeQos(qos);
        subscription.format = format;
        subscription.scriptId = sanitizedScriptId;
        session->subscriptions.append(subscription);
        entry = &session->subscriptions.last();
    } else {
        entry->alias = displayAlias;
        entry->requestedQos = SessionConfig::sanitizeQos(qos);
        entry->format = format;
        entry->scriptId = sanitizedScriptId;
        entry->paused = false;
        entry->lastError.clear();
    }

    session->subscriptionFormats.insert(filter, format);
    if (session->client->state() == QMqttClient::Connected) {
        ensureSubscriptionActive(*session, *entry, true);
    }

    saveSessions();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit sessionsChanged();
    return true;
}

bool AppController::updateCurrentSubscription(const QString &topic, const QString &alias, const QString &scriptId)
{
    auto *session = currentSessionState();
    if (!session) {
        return false;
    }

    SubscriptionEntry *entry = subscriptionByTopic(session, topic.trimmed());
    if (!entry) {
        return false;
    }

    const QString sanitizedScriptId = scriptById(scriptId) ? scriptId : QString();
    const QString displayAlias = alias.trimmed();
    if (entry->alias == displayAlias && entry->scriptId == sanitizedScriptId) {
        return true;
    }

    entry->alias = displayAlias;
    entry->scriptId = sanitizedScriptId;
    saveSessions();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    return true;
}

void AppController::removeCurrentSubscription(const QString &topic)
{
    auto *session = currentSessionState();
    if (!session) {
        return;
    }

    const QString filter = topic.trimmed();
    if (filter.isEmpty()) {
        return;
    }

    const auto it = std::find_if(
        session->subscriptions.begin(),
        session->subscriptions.end(),
        [&filter](const SubscriptionEntry &entry) { return entry.topic == filter; });
    if (it == session->subscriptions.end()) {
        return;
    }

    if (it->runtimeSubscription) {
        it->runtimeSubscription->unsubscribe();
    } else if (session->client->state() == QMqttClient::Connected) {
        session->client->unsubscribe(QMqttTopicFilter(filter));
    }

    appendEvent(*session, QStringLiteral("Subscription"), QStringLiteral("Removed %1").arg(filter));
    session->subscriptionFormats.remove(filter);
    session->subscriptions.erase(it);
    saveSessions();
    emit currentSessionChanged();
    emit subscriptionsChanged();
}

void AppController::setCurrentSubscriptionPaused(const QString &topic, bool paused)
{
    auto *session = currentSessionState();
    if (!session) {
        return;
    }

    SubscriptionEntry *entry = subscriptionByTopic(session, topic.trimmed());
    if (!entry || entry->paused == paused) {
        return;
    }

    entry->paused = paused;
    if (paused) {
        entry->runtimeState = QStringLiteral("paused");
        if (entry->runtimeSubscription) {
            entry->runtimeSubscription->unsubscribe();
        } else if (session->client->state() == QMqttClient::Connected) {
            session->client->unsubscribe(QMqttTopicFilter(entry->topic));
        }
        appendEvent(*session, QStringLiteral("Subscription"), QStringLiteral("Paused %1").arg(entry->topic));
    } else {
        entry->lastError.clear();
        if (session->client->state() == QMqttClient::Connected) {
            ensureSubscriptionActive(*session, *entry, true);
        } else {
            appendEvent(*session, QStringLiteral("Subscription"), QStringLiteral("Queued %1 for reconnect").arg(entry->topic));
        }
    }

    saveSessions();
    emit currentSessionChanged();
    emit subscriptionsChanged();
}

void AppController::publishCurrentSession(
    const QString &topic,
    const QString &payload,
    int format,
    int qos,
    bool retain)
{
    auto *session = currentSessionState();
    if (!session || !session->client) {
        return;
    }

    const QString trimmedTopic = topic.trimmed();
    if (trimmedTopic.isEmpty()) {
        appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Topic cannot be empty."));
        return;
    }

    const QMqttTopicName topicName(trimmedTopic);
    if (!topicName.isValid()) {
        appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Invalid topic name: %1").arg(trimmedTopic));
        return;
    }

    if (session->client->state() != QMqttClient::Connected) {
        appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Connect before publishing."));
        return;
    }

    QByteArray payloadBytes;
    QString error;
    const PayloadFormat payloadFormat = PayloadCodec::formatFromInt(format);
    if (!PayloadCodec::encodeForPublish(payloadFormat, payload, payloadBytes, error)) {
        appendEvent(
            *session,
            QStringLiteral("Publish"),
            QStringLiteral("%1 (%2)").arg(error, PayloadCodec::formatName(payloadFormat)));
        return;
    }

    QVariantMap status = defaultPublishStatus();
    status.insert(QStringLiteral("state"), QStringLiteral("queued"));
    status.insert(QStringLiteral("topic"), trimmedTopic);
    status.insert(QStringLiteral("qos"), SessionConfig::sanitizeQos(qos));
    status.insert(QStringLiteral("retain"), retain);
    status.insert(QStringLiteral("format"), format);
    status.insert(QStringLiteral("formatName"), PayloadCodec::formatName(payloadFormat));
    status.insert(QStringLiteral("reason"), QString());
    status.insert(QStringLiteral("updatedAt"), timestampNow());
    session->publishStatus = status;

    const qint32 messageId = session->client->publish(topicName, payloadBytes, SessionConfig::sanitizeQos(qos), retain);
    if (messageId < 0) {
        updatePublishStatus(*session, QStringLiteral("failed"), QStringLiteral("Qt MQTT rejected the publish request."));
        appendEvent(*session, QStringLiteral("Publish"), QStringLiteral("Publish rejected for %1").arg(trimmedTopic));
    } else {
        updatePublishStatus(*session, QStringLiteral("queued"), QString(), messageId);
        appendEvent(
            *session,
            QStringLiteral("Publish"),
            QStringLiteral("Queued %1 (QoS %2%3)")
                .arg(trimmedTopic)
                .arg(SessionConfig::sanitizeQos(qos))
                .arg(retain ? QStringLiteral(", retain") : QString()));
    }

    emit currentSessionChanged();
}

void AppController::clearCurrentMessages()
{
    auto *session = currentSessionState();
    if (!session) {
        return;
    }

    m_historyStore.clearMessages(session->id);
    session->eventRows.clear();
    session->oldestLoadedEventId = 0;
    session->loadedAllEventHistory = true;
    emit eventStreamChanged();
    emit scriptTestSamplesChanged();
}

QVariantList AppController::loadOlderCurrentEventRows()
{
    auto *session = currentSessionState();
    if (!session || session->loadedAllEventHistory || session->oldestLoadedEventId <= 0) {
        return {};
    }

    QVariantList rows = EventRenderer::loadHistoryRows(
        m_historyStore.loadEntriesBefore(session->id, session->oldestLoadedEventId, kEventPageSize),
        session->subscriptionFormats,
        m_launchTimestamp,
        false);
    if (rows.isEmpty()) {
        session->loadedAllEventHistory = true;
        return {};
    }

    if (EventRenderer::containsRowsBeforeLaunch(rows, m_launchTimestamp)
            && EventRenderer::startsWithCurrentLaunchRows(session->eventRows, m_launchTimestamp)
            && !EventRenderer::containsLaunchDivider(session->eventRows)) {
        rows.append(EventRenderer::launchDividerRow(m_launchTimestamp));
    }

    QVariantList merged;
    merged.reserve(rows.size() + session->eventRows.size());
    for (const QVariant &item : rows) {
        merged.append(item);
    }
    for (const QVariant &item : session->eventRows) {
        merged.append(item);
    }
    session->eventRows = merged;
    session->oldestLoadedEventId = EventRenderer::firstHistoryId(session->eventRows);
    emit scriptTestSamplesChanged();
    return rows;
}

QString AppController::upsertScript(const QString &id, const QString &name, const QString &code)
{
    const QString trimmedName = name.trimmed();
    const QString scriptName = trimmedName.isEmpty() ? QStringLiteral("Untitled Script") : trimmedName;
    const QString scriptCode = code.trimmed().isEmpty() ? ScriptStore::defaultLuaScript() : code;
    const QString scriptId = id.trimmed().isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id.trimmed();
    const QString updatedAt = timestampNow();
    const QVector<ScriptEntry> previousScripts = m_scripts;

    for (auto &script : m_scripts) {
        if (script.id == scriptId) {
            script.name = scriptName;
            script.code = scriptCode;
            script.updatedAt = updatedAt;
            if (script.fileName.isEmpty()) {
                script.fileName = ScriptStore::scriptFileNameForId(script.id);
            }
            if (!saveScripts()) {
                m_scripts = previousScripts;
                return QString();
            }
            emit scriptLibraryChanged();
            emit currentSessionChanged();
            emit subscriptionsChanged();
            return script.id;
        }
    }

    ScriptEntry script;
    script.id = scriptId;
    script.name = scriptName;
    script.code = scriptCode;
    script.updatedAt = updatedAt;
    script.fileName = ScriptStore::scriptFileNameForId(script.id);
    m_scripts.append(script);

    if (!saveScripts()) {
        m_scripts = previousScripts;
        return QString();
    }
    emit scriptLibraryChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    return script.id;
}

bool AppController::deleteScript(const QString &id)
{
    const QString scriptId = id.trimmed();
    if (scriptId.isEmpty()) {
        return false;
    }

    const QVector<ScriptEntry> previousScripts = m_scripts;
    QVector<QString> previousSubscriptionScriptIds;
    for (const auto &session : m_sessions) {
        for (const auto &subscription : session.subscriptions) {
            previousSubscriptionScriptIds.append(subscription.scriptId);
        }
    }

    bool removed = false;
    QString removedFileName;
    for (int i = 0; i < m_scripts.size(); ++i) {
        if (m_scripts.at(i).id == scriptId) {
            removedFileName = m_scripts.at(i).fileName;
            m_scripts.removeAt(i);
            removed = true;
            break;
        }
    }
    if (!removed) {
        return false;
    }

    for (auto &session : m_sessions) {
        for (auto &subscription : session.subscriptions) {
            if (subscription.scriptId == scriptId) {
                subscription.scriptId.clear();
            }
        }
    }

    if (!saveScripts()) {
        m_scripts = previousScripts;
        int subscriptionIndex = 0;
        for (auto &session : m_sessions) {
            for (auto &subscription : session.subscriptions) {
                subscription.scriptId = previousSubscriptionScriptIds.value(subscriptionIndex);
                ++subscriptionIndex;
            }
        }
        return false;
    }
    ScriptStore::removeScriptFile(removedFileName);
    saveSessions();
    emit scriptLibraryChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit sessionsChanged();
    return true;
}

QVariantMap AppController::testScript(const QString &code, const QString &topic, const QString &payload, int format) const
{
    QString encodeError;
    QByteArray payloadBytes;
    const PayloadFormat payloadFormat = PayloadCodec::formatFromInt(format);
    if (!PayloadCodec::encodeForPublish(payloadFormat, payload, payloadBytes, encodeError)) {
        payloadBytes = payload.toUtf8();
    }

    QString decodeError;
    const QString decoded = PayloadCodec::decodeForDisplay(payloadFormat, payloadBytes, decodeError);

    LuaScriptContext context;
    context.topic = topic.trimmed().isEmpty() ? QStringLiteral("test/topic") : topic.trimmed();
    context.payloadBytes = payloadBytes;
    context.decodedPayload = decoded;
    context.decodeError = decodeError;
    context.format = payloadFormat;
    context.timestamp = timestampNow();

    const LuaScriptResult result = LuaRunner::run(code, context);
    QVariantMap row;
    row.insert(QStringLiteral("success"), result.success);
    row.insert(QStringLiteral("output"), result.output);
    row.insert(QStringLiteral("error"), result.error);
    if (!encodeError.isEmpty()) {
        row.insert(QStringLiteral("inputError"), encodeError);
    }
    return row;
}

AppController::SessionState *AppController::currentSessionState()
{
    if (m_currentSessionIndex < 0 || m_currentSessionIndex >= m_sessions.size()) {
        return nullptr;
    }
    return &m_sessions[m_currentSessionIndex];
}

const AppController::SessionState *AppController::currentSessionState() const
{
    if (m_currentSessionIndex < 0 || m_currentSessionIndex >= m_sessions.size()) {
        return nullptr;
    }
    return &m_sessions[m_currentSessionIndex];
}

AppController::SessionState *AppController::sessionById(const QString &sessionId)
{
    for (auto &session : m_sessions) {
        if (session.id == sessionId) {
            return &session;
        }
    }
    return nullptr;
}

const AppController::SessionState *AppController::sessionById(const QString &sessionId) const
{
    for (const auto &session : m_sessions) {
        if (session.id == sessionId) {
            return &session;
        }
    }
    return nullptr;
}

AppController::SubscriptionEntry *AppController::subscriptionByTopic(SessionState *session, const QString &topic)
{
    if (!session) {
        return nullptr;
    }
    for (auto &entry : session->subscriptions) {
        if (entry.topic == topic) {
            return &entry;
        }
    }
    return nullptr;
}

const AppController::SubscriptionEntry *AppController::subscriptionByTopic(const SessionState *session, const QString &topic) const
{
    if (!session) {
        return nullptr;
    }
    for (const auto &entry : session->subscriptions) {
        if (entry.topic == topic) {
            return &entry;
        }
    }
    return nullptr;
}

const AppController::SubscriptionEntry *AppController::bestSubscriptionForTopic(
    const SessionState &session,
    const QString &topic) const
{
    const SubscriptionEntry *best = nullptr;
    int bestScore = -1;
    for (const auto &entry : session.subscriptions) {
        if (!PayloadCodec::topicFilterMatches(entry.topic, topic)) {
            continue;
        }
        const int score = topicSpecificityScore(entry.topic);
        if (score > bestScore || (score == bestScore && (!best || entry.topic < best->topic))) {
            bestScore = score;
            best = &entry;
        }
    }
    return best;
}

const AppController::ScriptEntry *AppController::scriptById(const QString &id) const
{
    if (id.trimmed().isEmpty()) {
        return nullptr;
    }
    for (const auto &script : m_scripts) {
        if (script.id == id) {
            return &script;
        }
    }
    return nullptr;
}

QString AppController::scriptName(const QString &id) const
{
    const auto *script = scriptById(id);
    return script ? script->name : QString();
}

void AppController::bindSessionSignals(SessionState *session)
{
    if (!session || !session->client) {
        return;
    }

    connect(session->client, &QMqttClient::connected, this, [this, sessionId = session->id]() {
        if (auto *boundSession = sessionById(sessionId)) {
            if (boundSession->connectTimeoutTimer) {
                boundSession->connectTimeoutTimer->stop();
            }
            boundSession->disconnectRequested = false;
            boundSession->lastError.clear();
            boundSession->brokerInfo =
                QStringLiteral("%1 • %2 • client %3")
                    .arg(protocolVersionLabel(boundSession->protocolVersion))
                    .arg(transportLabel(boundSession->transport))
                    .arg(boundSession->client->clientId());
            appendEvent(*boundSession, QStringLiteral("Connection"), QStringLiteral("Connected to broker"));
            restoreActiveSubscriptions(*boundSession, false);
        }

        emit sessionsChanged();
        emit currentSessionChanged();
        emit subscriptionsChanged();
    });

    connect(session->client, &QMqttClient::disconnected, this, [this, sessionId = session->id]() {
        if (auto *boundSession = sessionById(sessionId)) {
            if (boundSession->connectTimeoutTimer) {
                boundSession->connectTimeoutTimer->stop();
            }
            const QString message = boundSession->disconnectRequested
                ? QStringLiteral("Disconnected")
                : QStringLiteral("Connection closed by broker");
            boundSession->disconnectRequested = false;
            appendEvent(*boundSession, QStringLiteral("Connection"), message);
        }

        emit sessionsChanged();
        emit currentSessionChanged();
        emit subscriptionsChanged();
    });

    connect(session->client, &QMqttClient::stateChanged, this, [this]() {
        emit sessionsChanged();
        emit currentSessionChanged();
        emit subscriptionsChanged();
    });

    connect(
        session->client,
        &QMqttClient::errorChanged,
        this,
        [this, sessionId = session->id](QMqttClient::ClientError error) {
            if (error == QMqttClient::NoError) {
                return;
            }

            if (auto *boundSession = sessionById(sessionId)) {
                QString message = clientErrorName(error);
                const QString socketText = socketDiagnostic(boundSession->client);
                if (!socketText.isEmpty() && socketText != message) {
                    message = QStringLiteral("%1 (%2)").arg(message, socketText);
                }
                boundSession->lastError = message;
                appendEvent(*boundSession, QStringLiteral("Error"), message);
            }

            emit sessionsChanged();
            emit currentSessionChanged();
        });

    connect(session->client, &QMqttClient::brokerSessionRestored, this, [this, sessionId = session->id]() {
        if (auto *boundSession = sessionById(sessionId)) {
            boundSession->sessionRestored = true;
            appendEvent(*boundSession, QStringLiteral("Connection"), QStringLiteral("Broker session restored"));
        }
        emit sessionsChanged();
        emit currentSessionChanged();
    });

    connect(
        session->client,
        &QMqttClient::messageReceived,
        this,
        [this, sessionId = session->id](const QByteArray &message, const QMqttTopicName &topic) {
            appendIncomingMessage(sessionId, topic.name(), message);
        });

    connect(session->client, &QMqttClient::messageSent, this, [this, sessionId = session->id](qint32 messageId) {
        if (auto *boundSession = sessionById(sessionId)) {
            if (boundSession->publishStatus.value(QStringLiteral("messageId")).toInt() == messageId) {
                updatePublishStatus(*boundSession, QStringLiteral("sent"), QString(), messageId);
            }
        }
        emit currentSessionChanged();
    });

    connect(
        session->client,
        &QMqttClient::messageStatusChanged,
        this,
        [this, sessionId = session->id](
            qint32 messageId,
            QMqtt::MessageStatus status,
            const QMqttMessageStatusProperties &properties) {
            if (auto *boundSession = sessionById(sessionId)) {
                if (boundSession->publishStatus.value(QStringLiteral("messageId")).toInt() != messageId) {
                    return;
                }

                QString reason = properties.reason();
                if (reason.isEmpty()) {
                    reason = boundSession->publishStatus.value(QStringLiteral("reason")).toString();
                }
                updatePublishStatus(*boundSession, messageStatusName(status), reason, messageId);
            }
            emit currentSessionChanged();
        });

    connect(session->client, &QMqttClient::pingResponseReceived, this, [this, sessionId = session->id]() {
        if (auto *boundSession = sessionById(sessionId)) {
            boundSession->brokerInfo =
                QStringLiteral("%1 • %2 • ping ok")
                    .arg(protocolVersionLabel(boundSession->protocolVersion))
                    .arg(transportLabel(boundSession->transport));
        }
        emit sessionsChanged();
        emit currentSessionChanged();
    });
}

void AppController::initializeSessionRuntime(SessionState *session)
{
    if (!session) {
        return;
    }

    if (!session->connectTimeoutTimer) {
        session->connectTimeoutTimer = new QTimer(this);
        session->connectTimeoutTimer->setSingleShot(true);
        connect(session->connectTimeoutTimer, &QTimer::timeout, this, [this, sessionId = session->id]() {
            auto *boundSession = sessionById(sessionId);
            if (!boundSession || !boundSession->client || boundSession->client->state() != QMqttClient::Connecting) {
                return;
            }

            boundSession->lastError = QStringLiteral("Connection timed out.");
            appendEvent(*boundSession, QStringLiteral("Error"), boundSession->lastError);
            boundSession->client->disconnectFromHost();
            emit sessionsChanged();
            emit currentSessionChanged();
        });
    }

}

void AppController::connectSession(SessionState &session, const QString &eventPrefix)
{
    if (!session.client) {
        return;
    }

    if (session.connectTimeoutTimer) {
        session.connectTimeoutTimer->start(qMax(1, session.connectTimeoutSeconds) * 1000);
    }

    appendEvent(
        session,
        QStringLiteral("Connection"),
        QStringLiteral("%1 %2:%3 over %4 using %5")
            .arg(eventPrefix)
            .arg(session.client->hostname())
            .arg(session.client->port())
            .arg(transportLabel(session.transport))
            .arg(protocolVersionLabel(session.protocolVersion)));

    if (session.transport == QStringLiteral("tls")) {
        QString tlsError;
        const QSslConfiguration configuration = sslConfigurationForSession(session, tlsError);
        if (!tlsError.isEmpty()) {
            if (session.connectTimeoutTimer) {
                session.connectTimeoutTimer->stop();
            }
            session.lastError = tlsError;
            appendEvent(session, QStringLiteral("Error"), tlsError);
            return;
        }
        session.client->connectToHostEncrypted(configuration);
    } else {
        session.client->connectToHost();
    }
}

QSslConfiguration AppController::sslConfigurationForSession(const SessionState &session, QString &errorMessage) const
{
    QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
    configuration.setPeerVerifyMode(session.sslSecure ? QSslSocket::AutoVerifyPeer : QSslSocket::VerifyNone);

    const QList<QByteArray> protocols = alpnProtocols(session.alpn);
    if (!protocols.isEmpty()) {
        configuration.setAllowedNextProtocols(protocols);
    }

    const QString caPath = session.caFile.trimmed();
    if (!caPath.isEmpty()) {
        const QList<QSslCertificate> certificates = QSslCertificate::fromPath(caPath);
        if (certificates.isEmpty()) {
            errorMessage = QStringLiteral("CA certificate file could not be loaded.");
            return configuration;
        }
        configuration.setCaCertificates(certificates);
    }

    const QString certificatePath = session.clientCertificateFile.trimmed();
    if (!certificatePath.isEmpty()) {
        const QList<QSslCertificate> certificates = QSslCertificate::fromPath(certificatePath);
        if (certificates.isEmpty()) {
            errorMessage = QStringLiteral("Client certificate file could not be loaded.");
            return configuration;
        }
        configuration.setLocalCertificate(certificates.first());
    }

    const QString keyPath = session.clientKeyFile.trimmed();
    if (!keyPath.isEmpty()) {
        const QSslKey privateKey = readPrivateKey(keyPath);
        if (privateKey.isNull()) {
            errorMessage = QStringLiteral("Client key file could not be loaded.");
            return configuration;
        }
        configuration.setPrivateKey(privateKey);
    }

    return configuration;
}

void AppController::configureSession(SessionState &session, const QVariantMap &config, bool keepNameFallback)
{
    QString name = config.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty() && !keepNameFallback) {
        name = session.name;
    }
    if (!name.isEmpty()) {
        session.name = name;
    }

    session.transport = SessionConfig::sanitizeTransport(config.value(QStringLiteral("transport")));
    session.protocolVersion = SessionConfig::sanitizeProtocolVersion(config.value(QStringLiteral("protocolVersion"), 5));
    session.sslSecure = config.value(QStringLiteral("sslSecure"), true).toBool();
    session.alpn = config.value(QStringLiteral("alpn")).toString().trimmed();
    session.certificateType = config.value(QStringLiteral("certificateType"), QStringLiteral("ca")).toString() == QStringLiteral("self")
        ? QStringLiteral("self")
        : QStringLiteral("ca");
    session.caFile = config.value(QStringLiteral("caFile")).toString().trimmed();
    session.clientCertificateFile = config.value(QStringLiteral("clientCertificateFile")).toString().trimmed();
    session.clientKeyFile = config.value(QStringLiteral("clientKeyFile")).toString().trimmed();
    session.connectTimeoutSeconds =
        SessionConfig::sanitizeBoundedInt(config.value(QStringLiteral("connectTimeoutSeconds"), 10), 10, 1, 300);
    session.sessionExpiryInterval = SessionConfig::sanitizeOptionalUInt32(config.value(QStringLiteral("sessionExpiryInterval"), 0));
    session.receiveMaximum = SessionConfig::sanitizeOptionalUInt16(config.value(QStringLiteral("receiveMaximum")));
    session.maximumPacketSize = SessionConfig::sanitizeOptionalUInt32(config.value(QStringLiteral("maximumPacketSize")));
    session.topicAliasMaximum = SessionConfig::sanitizeOptionalUInt16(config.value(QStringLiteral("topicAliasMaximum")));
    session.requestResponseInformation =
        config.value(QStringLiteral("requestResponseInformation"), false).toBool();
    session.requestProblemInformation =
        config.value(QStringLiteral("requestProblemInformation"), false).toBool();
    session.authenticationMethod = config.value(QStringLiteral("authenticationMethod")).toString().trimmed();
    session.authenticationData = config.value(QStringLiteral("authenticationData")).toString();

    QString host = config.value(QStringLiteral("host")).toString().trimmed();
    if (host.isEmpty()) {
        host = QStringLiteral("broker.emqx.io");
    }

    session.client->setHostname(host);
    session.client->setPort(SessionConfig::sanitizePort(config.value(QStringLiteral("port")), session.transport));
    session.client->setProtocolVersion(toProtocolVersion(session.protocolVersion));

    QString clientId = config.value(QStringLiteral("clientId")).toString().trimmed();
    if (clientId.isEmpty()) {
        clientId = SessionConfig::generateClientId();
    }
    session.client->setClientId(clientId);
    session.client->setUsername(config.value(QStringLiteral("username")).toString());
    session.client->setPassword(config.value(QStringLiteral("password")).toString());
    session.client->setCleanSession(config.value(QStringLiteral("cleanSession"), true).toBool());
    session.client->setKeepAlive(SessionConfig::sanitizeKeepAlive(
        config.value(QStringLiteral("keepAliveSeconds"), SessionConfig::kDefaultKeepAlive)));
    session.client->setAutoKeepAlive(true);
    if (session.connectTimeoutTimer) {
        session.connectTimeoutTimer->setInterval(session.connectTimeoutSeconds * 1000);
    }
    QMqttConnectionProperties connectionProperties;
    connectionProperties.setSessionExpiryInterval(session.sessionExpiryInterval);
    if (session.receiveMaximum > 0) {
        connectionProperties.setMaximumReceive(session.receiveMaximum);
    }
    if (session.maximumPacketSize > 0) {
        connectionProperties.setMaximumPacketSize(session.maximumPacketSize);
    }
    if (session.topicAliasMaximum > 0) {
        connectionProperties.setMaximumTopicAlias(session.topicAliasMaximum);
    }
    connectionProperties.setRequestResponseInformation(session.requestResponseInformation);
    connectionProperties.setRequestProblemInformation(session.requestProblemInformation);
    if (!session.authenticationMethod.isEmpty()) {
        connectionProperties.setAuthenticationMethod(session.authenticationMethod);
        connectionProperties.setAuthenticationData(session.authenticationData.toUtf8());
    }
    session.client->setConnectionProperties(connectionProperties);
}

void AppController::restoreActiveSubscriptions(SessionState &session, bool emitEvents)
{
    for (auto &entry : session.subscriptions) {
        if (!entry.paused) {
            ensureSubscriptionActive(session, entry, emitEvents);
        }
    }
}

void AppController::ensureSubscriptionActive(SessionState &session, SubscriptionEntry &entry, bool emitEvents)
{
    if (entry.paused || !session.client || session.client->state() != QMqttClient::Connected) {
        return;
    }

    const QMqttTopicFilter filter(entry.topic);
    if (!filter.isValid()) {
        entry.runtimeState = QStringLiteral("error");
        entry.lastError = QStringLiteral("Invalid topic filter.");
        if (emitEvents) {
            appendEvent(session, QStringLiteral("Subscription"), QStringLiteral("%1 is not a valid topic filter").arg(entry.topic));
        }
        return;
    }

    QMqttSubscription *subscription = session.client->subscribe(filter, SessionConfig::sanitizeQos(entry.requestedQos));
    if (!subscription) {
        entry.runtimeState = QStringLiteral("error");
        entry.lastError = QStringLiteral("Qt MQTT returned no subscription object.");
        if (emitEvents) {
            appendEvent(session, QStringLiteral("Subscription"), QStringLiteral("Failed to subscribe to %1").arg(entry.topic));
        }
        return;
    }

    entry.runtimeSubscription = subscription;
    entry.runtimeState = subscriptionStateName(subscription->state());
    entry.grantedQos = subscription->qos();
    entry.lastError = subscription->reason();
    observeSubscription(session, entry, subscription);

    if (emitEvents) {
        appendEvent(
            session,
            QStringLiteral("Subscription"),
            QStringLiteral("Requested %1 at QoS %2").arg(entry.topic).arg(entry.requestedQos));
    }
}

void AppController::observeSubscription(SessionState &session, SubscriptionEntry &entry, QMqttSubscription *subscription)
{
    if (!subscription || subscription->property("mqttPlusObserved").toBool()) {
        return;
    }

    subscription->setProperty("mqttPlusObserved", true);
    connect(
        subscription,
        &QMqttSubscription::stateChanged,
        this,
        [this, sessionId = session.id, topic = entry.topic](QMqttSubscription::SubscriptionState state) {
            updateSubscriptionState(sessionId, topic, state);
        });
}

void AppController::updateSubscriptionState(
    const QString &sessionId,
    const QString &topic,
    QMqttSubscription::SubscriptionState state)
{
    auto *session = sessionById(sessionId);
    SubscriptionEntry *entry = subscriptionByTopic(session, topic);
    if (!session || !entry) {
        return;
    }

    const QString previousState = entry->runtimeState;
    entry->runtimeState = subscriptionStateName(state);
    if (entry->runtimeSubscription) {
        entry->grantedQos = entry->runtimeSubscription->qos();
        entry->lastError = entry->runtimeSubscription->reason();
    }

    if (state == QMqttSubscription::Subscribed) {
        entry->lastError.clear();
    }

    if (previousState != entry->runtimeState) {
        if (state == QMqttSubscription::Subscribed) {
            appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("Subscribed to %1").arg(entry->topic));
        } else if (state == QMqttSubscription::Unsubscribed && entry->paused) {
            appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("Paused %1").arg(entry->topic));
        } else if (state == QMqttSubscription::Error) {
            const QString reason = entry->lastError.isEmpty()
                ? QStringLiteral("Broker returned a subscription error.")
                : entry->lastError;
            appendEvent(
                *session,
                QStringLiteral("Subscription"),
                QStringLiteral("%1 failed: %2").arg(entry->topic, reason));
        }
    }

    emit subscriptionsChanged();
}

void AppController::updatePublishStatus(
    SessionState &session,
    const QString &state,
    const QString &reason,
    qint32 messageId)
{
    session.publishStatus.insert(QStringLiteral("state"), state);
    session.publishStatus.insert(QStringLiteral("reason"), reason);
    session.publishStatus.insert(QStringLiteral("updatedAt"), timestampNow());
    if (messageId >= 0) {
        session.publishStatus.insert(QStringLiteral("messageId"), messageId);
    }
}

void AppController::appendRenderedEventRow(SessionState &session, const QVariantMap &row)
{
    if (&session != currentSessionState()) {
        return;
    }

    session.eventRows.append(row);
    trimVisibleEventRows(session);
    emit eventStreamRowAppended(row);
    if (row.value(QStringLiteral("kind")).toString() == QStringLiteral("message")) {
        emit scriptTestSamplesChanged();
    }
}

void AppController::appendEvent(SessionState &session, const QString &channel, const QString &message)
{
    const QString timestamp = timestampNow();
    const qint64 historyId = m_historyStore.appendEvent(session.id, timestamp, channel, message);

    appendRenderedEventRow(session, EventRenderer::eventRow(historyId, timestamp, channel, message));
}

LuaScriptResult AppController::parseIncomingPayload(
    const SessionState &session,
    const SubscriptionEntry *subscription,
    const QString &topic,
    const QByteArray &payloadBytes,
    const QString &timestamp,
    QString &scriptNameOut,
    QString &decodedPayloadOut) const
{
    const PayloadFormat format = subscription
        ? PayloadCodec::formatFromInt(subscription->format)
        : PayloadCodec::resolveTopicFormat(session.subscriptionFormats, topic);

    QString decodeError;
    decodedPayloadOut = PayloadCodec::decodeForDisplay(format, payloadBytes, decodeError);

    if (!subscription || subscription->scriptId.isEmpty()) {
        return {};
    }

    const auto *script = scriptById(subscription->scriptId);
    if (!script) {
        LuaScriptResult result;
        result.error = QStringLiteral("Selected Lua script is missing.");
        return result;
    }

    scriptNameOut = script->name;

    LuaScriptContext context;
    context.topic = topic;
    context.payloadBytes = payloadBytes;
    context.decodedPayload = decodedPayloadOut;
    context.decodeError = decodeError;
    context.format = format;
    context.timestamp = timestamp;
    return LuaRunner::run(script->code, context);
}

void AppController::appendIncomingMessage(const QString &sessionId, const QString &topic, const QByteArray &payloadBytes)
{
    auto *session = sessionById(sessionId);
    if (!session) {
        return;
    }

    const QString timestamp = timestampNow();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    bool refreshCurrentSubscriptionFps = false;

    for (auto &subscription : session->subscriptions) {
        if (!PayloadCodec::topicFilterMatches(subscription.topic, topic)) {
            continue;
        }

        subscription.recentMessageTimestampsMs.append(nowMs);
        pruneRecentMessageTimestamps(subscription.recentMessageTimestampsMs, nowMs);
        refreshCurrentSubscriptionFps = refreshCurrentSubscriptionFps || session == currentSessionState();
    }

    const SubscriptionEntry *displaySubscription = bestSubscriptionForTopic(*session, topic);
    QString scriptDisplayName;
    QString decodedPayload;
    const LuaScriptResult scriptResult = parseIncomingPayload(
        *session,
        displaySubscription,
        topic,
        payloadBytes,
        timestamp,
        scriptDisplayName,
        decodedPayload);
    const bool hasScript = displaySubscription && !displaySubscription->scriptId.isEmpty();
    const QString scriptId = hasScript ? displaySubscription->scriptId : QString();
    const QString parsedFormat = hasScript && scriptResult.success
        ? QStringLiteral("Lua: %1").arg(scriptDisplayName)
        : QString();
    const QString parseError = hasScript && !scriptResult.success
        ? scriptResult.error
        : QString();

    const qint64 historyId = m_historyStore.appendMessage(
        sessionId,
        timestamp,
        topic,
        payloadBytes,
        hasScript && scriptResult.success ? scriptResult.output : QString(),
        parsedFormat,
        parseError,
        scriptId,
        scriptDisplayName);

    if (refreshCurrentSubscriptionFps && !m_subscriptionFpsRefreshTimer.isActive()) {
        m_subscriptionFpsRefreshTimer.start();
    }

    if (session != currentSessionState() || session->outputPaused) {
        return;
    }

    QVariantMap historyRow;
    historyRow.insert(QStringLiteral("id"), historyId);
    historyRow.insert(QStringLiteral("timestamp"), timestamp);
    historyRow.insert(QStringLiteral("entry_type"), QStringLiteral("message"));
    historyRow.insert(QStringLiteral("topic"), topic);
    historyRow.insert(QStringLiteral("payload"), QString::fromUtf8(payloadBytes));
    historyRow.insert(QStringLiteral("payload_b64"), QString::fromLatin1(payloadBytes.toBase64()));
    historyRow.insert(QStringLiteral("parsed_payload"), hasScript && scriptResult.success ? scriptResult.output : QString());
    historyRow.insert(QStringLiteral("parsed_format"), parsedFormat);
    historyRow.insert(QStringLiteral("parse_error"), parseError);
    historyRow.insert(QStringLiteral("script_id"), scriptId);
    historyRow.insert(QStringLiteral("script_name"), scriptDisplayName);
    appendRenderedEventRow(*session, EventRenderer::renderHistoryRow(historyRow, session->subscriptionFormats));
}

qreal AppController::subscriptionFps(const SubscriptionEntry &entry, qint64 nowMs) const
{
    return static_cast<qreal>(recentMessageCount(entry.recentMessageTimestampsMs, nowMs));
}

bool AppController::currentSessionHasActiveSubscriptionFps(qint64 nowMs) const
{
    const auto *session = currentSessionState();
    if (!session) {
        return false;
    }

    for (const auto &subscription : session->subscriptions) {
        if (recentMessageCount(subscription.recentMessageTimestampsMs, nowMs) > 0) {
            return true;
        }
    }

    return false;
}

void AppController::refreshSubscriptionFps()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!currentSessionHasActiveSubscriptionFps(nowMs)) {
        m_subscriptionFpsRefreshTimer.stop();
    }

    emit subscriptionsChanged();
}

void AppController::trimVisibleEventRows(SessionState &session)
{
    const qsizetype overflow = session.eventRows.size() - kMaxVisibleEventRows;
    if (overflow <= 0) {
        return;
    }

    session.eventRows.remove(0, overflow);
    session.oldestLoadedEventId = EventRenderer::firstHistoryId(session.eventRows);
    session.loadedAllEventHistory = false;
}

void AppController::reloadCurrentSessionHistory()
{
    auto *session = currentSessionState();
    if (!session) {
        return;
    }
    const QVariantList rows = m_historyStore.loadEntries(session->id, kEventPageSize);
    session->eventRows = EventRenderer::loadHistoryRows(
        rows,
        session->subscriptionFormats,
        m_launchTimestamp,
        true);
    session->oldestLoadedEventId = EventRenderer::firstHistoryId(session->eventRows);
    session->loadedAllEventHistory = rows.size() < kEventPageSize;
    emit scriptTestSamplesChanged();
}

void AppController::loadScripts()
{
    const ScriptStore::LoadResult result = ScriptStore::loadScripts();
    m_scripts = result.scripts;
    m_scriptIndexWritable = result.indexWritable;
}

bool AppController::saveScripts()
{
    return ScriptStore::saveScripts(m_scripts, m_scriptIndexWritable);
}

void AppController::loadSessions()
{
    const int count = m_settings.beginReadArray(QStringLiteral("sessions"));
    for (int i = 0; i < count; ++i) {
        SessionConfig::LoadedSession loaded = SessionConfig::readSessionSettings(
            m_settings,
            i,
            [this](const QString &scriptId) { return scriptById(scriptId) != nullptr; });
        SessionState session = loaded.session;

        session.client = new QMqttClient(this);
        session.client->setAutoKeepAlive(true);
        initializeSessionRuntime(&session);
        configureSession(session, loaded.config, false);
        session.publishStatus = defaultPublishStatus();
        bindSessionSignals(&session);
        m_sessions.append(session);
    }
    m_settings.endArray();

    if (m_sessions.isEmpty()) {
        m_sessions.append(createDefaultSession(QStringLiteral("Session 1")));
        saveSessions();
    }

    m_currentSessionIndex = 0;
    reloadCurrentSessionHistory();
    emit sessionsChanged();
    emit currentSessionIndexChanged();
    emit currentSessionChanged();
    emit subscriptionsChanged();
    emit eventStreamChanged();
}

void AppController::saveSessions()
{
    SessionConfig::writeSessionSettings(m_settings, m_sessions);
}

AppController::SessionState AppController::createDefaultSession(const QString &name) const
{
    SessionState session;
    session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.name = name;
    session.transport = QStringLiteral("tcp");
    session.protocolVersion = 5;
    session.publishStatus = defaultPublishStatus();
    session.client = new QMqttClient(const_cast<AppController *>(this));
    session.client->setAutoKeepAlive(true);
    const_cast<AppController *>(this)->initializeSessionRuntime(&session);
    QVariantMap config = SessionConfig::defaultConfig(1);
    config.insert(QStringLiteral("name"), name);
    const_cast<AppController *>(this)->configureSession(session, config, false);
    const_cast<AppController *>(this)->bindSessionSignals(&session);
    return session;
}

void AppController::refreshSystemColorScheme()
{
    const bool darkMode = QGuiApplication::styleHints()
        && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    if (darkMode == m_systemDarkMode) {
        return;
    }

    const QString previousEffectiveTheme = effectiveTheme();
    m_systemDarkMode = darkMode;
    if (effectiveTheme() != previousEffectiveTheme) {
        emit effectiveThemeChanged();
    }
}
