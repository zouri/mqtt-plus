#include "viewmodels/sessioneditorviewmodel.h"

#include "domain/sessionconfig.h"

#include <algorithm>
#include <QStringList>

SessionEditorViewModel::SessionEditorViewModel(QObject *parent)
    : QObject(parent)
{
    loadConfig(defaultConfig(1));
}

SessionConnectionConfig SessionEditorViewModel::defaultConfig(int sessionNumber)
{
    return SessionConfig::defaultConfig(sessionNumber);
}

bool SessionEditorViewModel::editMode() const
{
    return m_editMode;
}

int SessionEditorViewModel::targetIndex() const
{
    return m_targetIndex;
}

QString SessionEditorViewModel::title() const
{
    return m_title;
}

QString SessionEditorViewModel::name() const
{
    return m_name;
}

QString SessionEditorViewModel::host() const
{
    return m_host;
}

QString SessionEditorViewModel::portText() const
{
    return m_portText;
}

QString SessionEditorViewModel::connectTimeoutText() const
{
    return m_connectTimeoutText;
}

QString SessionEditorViewModel::transport() const
{
    return m_transport;
}

int SessionEditorViewModel::protocolVersion() const
{
    return m_protocolVersion;
}

bool SessionEditorViewModel::sslSecure() const
{
    return m_sslSecure;
}

QString SessionEditorViewModel::alpn() const
{
    return m_alpn;
}

QString SessionEditorViewModel::certificateType() const
{
    return m_certificateType;
}

QString SessionEditorViewModel::caFile() const
{
    return m_caFile;
}

QString SessionEditorViewModel::clientCertificateFile() const
{
    return m_clientCertificateFile;
}

QString SessionEditorViewModel::clientKeyFile() const
{
    return m_clientKeyFile;
}

QString SessionEditorViewModel::clientId() const
{
    return m_clientId;
}

QString SessionEditorViewModel::username() const
{
    return m_username;
}

QString SessionEditorViewModel::password() const
{
    return m_password;
}

QString SessionEditorViewModel::keepAliveText() const
{
    return m_keepAliveText;
}

bool SessionEditorViewModel::cleanSession() const
{
    return m_cleanSession;
}

QString SessionEditorViewModel::sessionExpiryText() const
{
    return m_sessionExpiryText;
}

QString SessionEditorViewModel::receiveMaximumText() const
{
    return m_receiveMaximumText;
}

QString SessionEditorViewModel::maximumPacketSizeText() const
{
    return m_maximumPacketSizeText;
}

QString SessionEditorViewModel::topicAliasMaximumText() const
{
    return m_topicAliasMaximumText;
}

bool SessionEditorViewModel::requestResponseInformation() const
{
    return m_requestResponseInformation;
}

bool SessionEditorViewModel::requestProblemInformation() const
{
    return m_requestProblemInformation;
}

QString SessionEditorViewModel::authenticationMethod() const
{
    return m_authenticationMethod;
}

QString SessionEditorViewModel::authenticationData() const
{
    return m_authenticationData;
}

QString SessionEditorViewModel::validationError() const
{
    return m_validationError;
}

void SessionEditorViewModel::setName(const QString &name)
{
    if (m_name == name) {
        return;
    }
    m_name = name;
    setValidationError(QString());
    emit nameChanged();
}

void SessionEditorViewModel::setHost(const QString &host)
{
    if (m_host == host) {
        return;
    }
    m_host = host;
    setValidationError(QString());
    emit hostChanged();
}

void SessionEditorViewModel::setPortText(const QString &portText)
{
    if (m_portText == portText) {
        return;
    }
    m_portText = portText;
    setValidationError(QString());
    emit portTextChanged();
}

void SessionEditorViewModel::setConnectTimeoutText(const QString &connectTimeoutText)
{
    if (m_connectTimeoutText == connectTimeoutText) {
        return;
    }
    m_connectTimeoutText = connectTimeoutText;
    setValidationError(QString());
    emit connectTimeoutTextChanged();
}

void SessionEditorViewModel::setTransport(const QString &transport)
{
    const QString normalized = SessionConfig::sanitizeTransport(transport);
    if (m_transport == normalized) {
        return;
    }
    m_transport = normalized;
    emit transportChanged();
    if (m_transport == QStringLiteral("tls") && m_portText.trimmed() == QStringLiteral("1883")) {
        setPortText(QStringLiteral("8883"));
    } else if (m_transport == QStringLiteral("tcp") && m_portText.trimmed() == QStringLiteral("8883")) {
        setPortText(QStringLiteral("1883"));
    }
}

void SessionEditorViewModel::setProtocolVersion(int protocolVersion)
{
    const int normalized = SessionConfig::sanitizeProtocolVersion(protocolVersion);
    if (m_protocolVersion == normalized) {
        return;
    }
    m_protocolVersion = normalized;
    emit protocolVersionChanged();
}

void SessionEditorViewModel::setSslSecure(bool sslSecure)
{
    if (m_sslSecure == sslSecure) {
        return;
    }
    m_sslSecure = sslSecure;
    emit sslSecureChanged();
}

void SessionEditorViewModel::setAlpn(const QString &alpn)
{
    if (m_alpn == alpn) {
        return;
    }
    m_alpn = alpn;
    emit alpnChanged();
}

void SessionEditorViewModel::setCertificateType(const QString &certificateType)
{
    const QString normalized = certificateType == QStringLiteral("self") ? QStringLiteral("self") : QStringLiteral("ca");
    if (m_certificateType == normalized) {
        return;
    }
    m_certificateType = normalized;
    emit certificateTypeChanged();
}

void SessionEditorViewModel::setCaFile(const QString &caFile)
{
    if (m_caFile == caFile) {
        return;
    }
    m_caFile = caFile;
    emit caFileChanged();
}

void SessionEditorViewModel::setClientCertificateFile(const QString &clientCertificateFile)
{
    if (m_clientCertificateFile == clientCertificateFile) {
        return;
    }
    m_clientCertificateFile = clientCertificateFile;
    emit clientCertificateFileChanged();
}

void SessionEditorViewModel::setClientKeyFile(const QString &clientKeyFile)
{
    if (m_clientKeyFile == clientKeyFile) {
        return;
    }
    m_clientKeyFile = clientKeyFile;
    emit clientKeyFileChanged();
}

void SessionEditorViewModel::setClientId(const QString &clientId)
{
    if (m_clientId == clientId) {
        return;
    }
    m_clientId = clientId;
    emit clientIdChanged();
}

void SessionEditorViewModel::setUsername(const QString &username)
{
    if (m_username == username) {
        return;
    }
    m_username = username;
    emit usernameChanged();
}

void SessionEditorViewModel::setPassword(const QString &password)
{
    if (m_password == password) {
        return;
    }
    m_password = password;
    emit passwordChanged();
}

void SessionEditorViewModel::setKeepAliveText(const QString &keepAliveText)
{
    if (m_keepAliveText == keepAliveText) {
        return;
    }
    m_keepAliveText = keepAliveText;
    setValidationError(QString());
    emit keepAliveTextChanged();
}

void SessionEditorViewModel::setCleanSession(bool cleanSession)
{
    if (m_cleanSession == cleanSession) {
        return;
    }
    m_cleanSession = cleanSession;
    emit cleanSessionChanged();
}

void SessionEditorViewModel::setSessionExpiryText(const QString &sessionExpiryText)
{
    if (m_sessionExpiryText == sessionExpiryText) {
        return;
    }
    m_sessionExpiryText = sessionExpiryText;
    setValidationError(QString());
    emit sessionExpiryTextChanged();
}

void SessionEditorViewModel::setReceiveMaximumText(const QString &receiveMaximumText)
{
    if (m_receiveMaximumText == receiveMaximumText) {
        return;
    }
    m_receiveMaximumText = receiveMaximumText;
    setValidationError(QString());
    emit receiveMaximumTextChanged();
}

void SessionEditorViewModel::setMaximumPacketSizeText(const QString &maximumPacketSizeText)
{
    if (m_maximumPacketSizeText == maximumPacketSizeText) {
        return;
    }
    m_maximumPacketSizeText = maximumPacketSizeText;
    setValidationError(QString());
    emit maximumPacketSizeTextChanged();
}

void SessionEditorViewModel::setTopicAliasMaximumText(const QString &topicAliasMaximumText)
{
    if (m_topicAliasMaximumText == topicAliasMaximumText) {
        return;
    }
    m_topicAliasMaximumText = topicAliasMaximumText;
    setValidationError(QString());
    emit topicAliasMaximumTextChanged();
}

void SessionEditorViewModel::setRequestResponseInformation(bool requestResponseInformation)
{
    if (m_requestResponseInformation == requestResponseInformation) {
        return;
    }
    m_requestResponseInformation = requestResponseInformation;
    emit requestResponseInformationChanged();
}

void SessionEditorViewModel::setRequestProblemInformation(bool requestProblemInformation)
{
    if (m_requestProblemInformation == requestProblemInformation) {
        return;
    }
    m_requestProblemInformation = requestProblemInformation;
    emit requestProblemInformationChanged();
}

void SessionEditorViewModel::setAuthenticationMethod(const QString &authenticationMethod)
{
    if (m_authenticationMethod == authenticationMethod) {
        return;
    }
    m_authenticationMethod = authenticationMethod;
    emit authenticationMethodChanged();
}

void SessionEditorViewModel::setAuthenticationData(const QString &authenticationData)
{
    if (m_authenticationData == authenticationData) {
        return;
    }
    m_authenticationData = authenticationData;
    emit authenticationDataChanged();
}

void SessionEditorViewModel::openForCreate(const SessionConnectionConfig &config)
{
    setEditMode(false);
    setTargetIndex(-1);
    setTitle(QStringLiteral("New Connection"));
    loadConfig(config);
}

void SessionEditorViewModel::openForEdit(
    int index,
    const SessionConnectionConfig &config)
{
    setEditMode(true);
    setTargetIndex(index);
    setTitle(QStringLiteral("Edit Connection"));
    loadConfig(config);
}

void SessionEditorViewModel::loadConfig(const SessionConnectionConfig &config)
{
    setName(config.name);
    setHost(config.host);
    setPortText(QString::number(config.port));
    setConnectTimeoutText(QString::number(config.connectTimeoutSeconds));
    setTransport(config.transport);
    setProtocolVersion(config.protocolVersion);
    setSslSecure(config.sslSecure);
    setAlpn(config.alpn);
    setCertificateType(config.certificateType);
    setCaFile(config.caFile);
    setClientCertificateFile(config.clientCertificateFile);
    setClientKeyFile(config.clientKeyFile);
    setClientId(config.clientId);
    setUsername(config.username);
    setPassword(config.password);
    setKeepAliveText(QString::number(config.keepAliveSeconds));
    setCleanSession(config.cleanSession);
    setSessionExpiryText(QString::number(config.sessionExpiryInterval));
    setReceiveMaximumText(config.receiveMaximum > 0
            ? QString::number(config.receiveMaximum)
            : QString());
    setMaximumPacketSizeText(config.maximumPacketSize > 0
            ? QString::number(config.maximumPacketSize)
            : QString());
    setTopicAliasMaximumText(config.topicAliasMaximum > 0
            ? QString::number(config.topicAliasMaximum)
            : QString());
    setRequestResponseInformation(config.requestResponseInformation);
    setRequestProblemInformation(config.requestProblemInformation);
    setAuthenticationMethod(config.authenticationMethod);
    setAuthenticationData(config.authenticationData);
    setValidationError(QString());
}

SessionConnectionConfig SessionEditorViewModel::collectedConfig() const
{
    SessionConnectionConfig config;
    config.name = m_name.trimmed();
    config.host = m_host.trimmed();
    config.port = m_portText.trimmed().toInt();
    config.connectTimeoutSeconds = m_connectTimeoutText.trimmed().toInt();
    config.transport = m_transport;
    config.protocolVersion = m_protocolVersion;
    config.sslSecure = m_sslSecure;
    config.alpn = m_alpn;
    config.certificateType = m_certificateType;
    config.caFile = m_caFile;
    config.clientCertificateFile = m_clientCertificateFile;
    config.clientKeyFile = m_clientKeyFile;
    config.clientId = m_clientId.trimmed();
    config.username = m_username;
    config.password = m_password;
    config.keepAliveSeconds = m_keepAliveText.trimmed().toInt();
    config.cleanSession = m_cleanSession;
    config.sessionExpiryInterval = SessionConfig::sanitizeOptionalUInt32(
        m_sessionExpiryText);
    config.receiveMaximum = SessionConfig::sanitizeOptionalUInt16(m_receiveMaximumText);
    config.maximumPacketSize = SessionConfig::sanitizeOptionalUInt32(m_maximumPacketSizeText);
    config.topicAliasMaximum = SessionConfig::sanitizeOptionalUInt16(m_topicAliasMaximumText);
    config.requestResponseInformation = m_requestResponseInformation;
    config.requestProblemInformation = m_requestProblemInformation;
    config.authenticationMethod = m_authenticationMethod;
    config.authenticationData = m_authenticationData;
    return config;
}

bool SessionEditorViewModel::validate()
{
    if (m_name.trimmed().isEmpty()) {
        setValidationError(QStringLiteral("Name is required."));
        return false;
    }
    if (m_host.trimmed().isEmpty()) {
        setValidationError(QStringLiteral("Server address is required."));
        return false;
    }

    const QStringList checks {
        integerValidationError(m_portText, QStringLiteral("Port"), 1, 65535, true),
        integerValidationError(m_connectTimeoutText, QStringLiteral("Connection timeout"), 1, 300, true),
        integerValidationError(m_keepAliveText, QStringLiteral("Keep Alive"), 5, 1200, true),
        integerValidationError(m_sessionExpiryText, QStringLiteral("Session expiry interval"), 0, 4294967295ULL, false),
        integerValidationError(m_receiveMaximumText, QStringLiteral("Receive maximum"), 1, 65535, false),
        integerValidationError(m_maximumPacketSizeText, QStringLiteral("Maximum packet size"), 1, 4294967295ULL, false),
        integerValidationError(m_topicAliasMaximumText, QStringLiteral("Topic alias maximum"), 1, 65535, false),
    };
    for (const QString &message : checks) {
        if (!message.isEmpty()) {
            setValidationError(message);
            return false;
        }
    }

    setValidationError(QString());
    return true;
}

QString SessionEditorViewModel::integerValidationError(const QString &text, const QString &label, quint64 minimum, quint64 maximum, bool required) const
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return required ? QStringLiteral("%1 is required.").arg(label) : QString();
    }

    bool ok = false;
    const quint64 value = trimmed.toULongLong(&ok);
    if (!ok) {
        return QStringLiteral("%1 must be an integer.").arg(label);
    }
    if (value < minimum || value > maximum) {
        return QStringLiteral("%1 must be between %2 and %3.").arg(label).arg(minimum).arg(maximum);
    }
    return {};
}

void SessionEditorViewModel::setEditMode(bool editMode)
{
    if (m_editMode == editMode) {
        return;
    }
    m_editMode = editMode;
    emit editModeChanged();
}

void SessionEditorViewModel::setTargetIndex(int targetIndex)
{
    if (m_targetIndex == targetIndex) {
        return;
    }
    m_targetIndex = targetIndex;
    emit targetIndexChanged();
}

void SessionEditorViewModel::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    emit titleChanged();
}

void SessionEditorViewModel::setValidationError(const QString &message)
{
    if (m_validationError == message) {
        return;
    }
    m_validationError = message;
    emit validationErrorChanged();
}
