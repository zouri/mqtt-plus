#pragma once

#include "domain/sessionconfig.h"

#include <QObject>
#include <QString>

class SessionEditorViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool editMode READ editMode NOTIFY editModeChanged)
    Q_PROPERTY(int targetIndex READ targetIndex NOTIFY targetIndexChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(QString portText READ portText WRITE setPortText NOTIFY portTextChanged)
    Q_PROPERTY(QString connectTimeoutText READ connectTimeoutText WRITE setConnectTimeoutText NOTIFY connectTimeoutTextChanged)
    Q_PROPERTY(QString transport READ transport WRITE setTransport NOTIFY transportChanged)
    Q_PROPERTY(int transportIndex READ transportIndex WRITE setTransportIndex NOTIFY transportChanged)
    Q_PROPERTY(QStringList transportSchemes READ transportSchemes CONSTANT)
    Q_PROPERTY(bool webSocketTransport READ webSocketTransport NOTIFY transportChanged)
    Q_PROPERTY(QString webSocketPath READ webSocketPath WRITE setWebSocketPath NOTIFY webSocketPathChanged)
    Q_PROPERTY(int protocolVersion READ protocolVersion WRITE setProtocolVersion NOTIFY protocolVersionChanged)
    Q_PROPERTY(bool sslSecure READ sslSecure WRITE setSslSecure NOTIFY sslSecureChanged)
    Q_PROPERTY(QString alpn READ alpn WRITE setAlpn NOTIFY alpnChanged)
    Q_PROPERTY(QString certificateType READ certificateType WRITE setCertificateType NOTIFY certificateTypeChanged)
    Q_PROPERTY(QString caFile READ caFile WRITE setCaFile NOTIFY caFileChanged)
    Q_PROPERTY(QString clientCertificateFile READ clientCertificateFile WRITE setClientCertificateFile NOTIFY clientCertificateFileChanged)
    Q_PROPERTY(QString clientKeyFile READ clientKeyFile WRITE setClientKeyFile NOTIFY clientKeyFileChanged)
    Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY clientIdChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(QString keepAliveText READ keepAliveText WRITE setKeepAliveText NOTIFY keepAliveTextChanged)
    Q_PROPERTY(bool cleanSession READ cleanSession WRITE setCleanSession NOTIFY cleanSessionChanged)
    Q_PROPERTY(QString sessionExpiryText READ sessionExpiryText WRITE setSessionExpiryText NOTIFY sessionExpiryTextChanged)
    Q_PROPERTY(QString receiveMaximumText READ receiveMaximumText WRITE setReceiveMaximumText NOTIFY receiveMaximumTextChanged)
    Q_PROPERTY(QString maximumPacketSizeText READ maximumPacketSizeText WRITE setMaximumPacketSizeText NOTIFY maximumPacketSizeTextChanged)
    Q_PROPERTY(QString topicAliasMaximumText READ topicAliasMaximumText WRITE setTopicAliasMaximumText NOTIFY topicAliasMaximumTextChanged)
    Q_PROPERTY(bool requestResponseInformation READ requestResponseInformation WRITE setRequestResponseInformation NOTIFY requestResponseInformationChanged)
    Q_PROPERTY(bool requestProblemInformation READ requestProblemInformation WRITE setRequestProblemInformation NOTIFY requestProblemInformationChanged)
    Q_PROPERTY(QString authenticationMethod READ authenticationMethod WRITE setAuthenticationMethod NOTIFY authenticationMethodChanged)
    Q_PROPERTY(QString authenticationData READ authenticationData WRITE setAuthenticationData NOTIFY authenticationDataChanged)
    Q_PROPERTY(QString userPropertiesText READ userPropertiesText WRITE setUserPropertiesText NOTIFY userPropertiesTextChanged)
    Q_PROPERTY(bool willEnabled READ willEnabled WRITE setWillEnabled NOTIFY willEnabledChanged)
    Q_PROPERTY(QString willTopic READ willTopic WRITE setWillTopic NOTIFY willTopicChanged)
    Q_PROPERTY(QString willPayload READ willPayload WRITE setWillPayload NOTIFY willPayloadChanged)
    Q_PROPERTY(int willPayloadFormat READ willPayloadFormat WRITE setWillPayloadFormat NOTIFY willPayloadFormatChanged)
    Q_PROPERTY(int willQos READ willQos WRITE setWillQos NOTIFY willQosChanged)
    Q_PROPERTY(bool willRetain READ willRetain WRITE setWillRetain NOTIFY willRetainChanged)
    Q_PROPERTY(QString willDelayText READ willDelayText WRITE setWillDelayText NOTIFY willDelayTextChanged)
    Q_PROPERTY(bool willPayloadUtf8 READ willPayloadUtf8 WRITE setWillPayloadUtf8 NOTIFY willPayloadUtf8Changed)
    Q_PROPERTY(QString willExpiryText READ willExpiryText WRITE setWillExpiryText NOTIFY willExpiryTextChanged)
    Q_PROPERTY(QString willContentType READ willContentType WRITE setWillContentType NOTIFY willContentTypeChanged)
    Q_PROPERTY(QString willResponseTopic READ willResponseTopic WRITE setWillResponseTopic NOTIFY willResponseTopicChanged)
    Q_PROPERTY(QString willCorrelationDataBase64 READ willCorrelationDataBase64 WRITE setWillCorrelationDataBase64 NOTIFY willCorrelationDataBase64Changed)
    Q_PROPERTY(QString willUserPropertiesText READ willUserPropertiesText WRITE setWillUserPropertiesText NOTIFY willUserPropertiesTextChanged)
    Q_PROPERTY(QString validationError READ validationError NOTIFY validationErrorChanged)

public:
    explicit SessionEditorViewModel(QObject *parent = nullptr);

    static SessionConnectionConfig defaultConfig(int sessionNumber);

    bool editMode() const;
    int targetIndex() const;
    QString title() const;
    QString name() const;
    QString host() const;
    QString portText() const;
    QString connectTimeoutText() const;
    QString transport() const;
    int transportIndex() const;
    QStringList transportSchemes() const;
    bool webSocketTransport() const;
    QString webSocketPath() const;
    int protocolVersion() const;
    bool sslSecure() const;
    QString alpn() const;
    QString certificateType() const;
    QString caFile() const;
    QString clientCertificateFile() const;
    QString clientKeyFile() const;
    QString clientId() const;
    QString username() const;
    QString password() const;
    QString keepAliveText() const;
    bool cleanSession() const;
    QString sessionExpiryText() const;
    QString receiveMaximumText() const;
    QString maximumPacketSizeText() const;
    QString topicAliasMaximumText() const;
    bool requestResponseInformation() const;
    bool requestProblemInformation() const;
    QString authenticationMethod() const;
    QString authenticationData() const;
    QString userPropertiesText() const;
    bool willEnabled() const;
    QString willTopic() const;
    QString willPayload() const;
    int willPayloadFormat() const;
    int willQos() const;
    bool willRetain() const;
    QString willDelayText() const;
    bool willPayloadUtf8() const;
    QString willExpiryText() const;
    QString willContentType() const;
    QString willResponseTopic() const;
    QString willCorrelationDataBase64() const;
    QString willUserPropertiesText() const;
    QString validationError() const;

    void setName(const QString &name);
    void setHost(const QString &host);
    void setPortText(const QString &portText);
    void setConnectTimeoutText(const QString &connectTimeoutText);
    void setTransport(const QString &transport);
    void setTransportIndex(int index);
    void setWebSocketPath(const QString &webSocketPath);
    void setProtocolVersion(int protocolVersion);
    void setSslSecure(bool sslSecure);
    void setAlpn(const QString &alpn);
    void setCertificateType(const QString &certificateType);
    void setCaFile(const QString &caFile);
    void setClientCertificateFile(const QString &clientCertificateFile);
    void setClientKeyFile(const QString &clientKeyFile);
    void setClientId(const QString &clientId);
    void setUsername(const QString &username);
    void setPassword(const QString &password);
    void setKeepAliveText(const QString &keepAliveText);
    void setCleanSession(bool cleanSession);
    void setSessionExpiryText(const QString &sessionExpiryText);
    void setReceiveMaximumText(const QString &receiveMaximumText);
    void setMaximumPacketSizeText(const QString &maximumPacketSizeText);
    void setTopicAliasMaximumText(const QString &topicAliasMaximumText);
    void setRequestResponseInformation(bool requestResponseInformation);
    void setRequestProblemInformation(bool requestProblemInformation);
    void setAuthenticationMethod(const QString &authenticationMethod);
    void setAuthenticationData(const QString &authenticationData);
    void setUserPropertiesText(const QString &text);
    void setWillEnabled(bool enabled);
    void setWillTopic(const QString &topic);
    void setWillPayload(const QString &payload);
    void setWillPayloadFormat(int format);
    void setWillQos(int qos);
    void setWillRetain(bool retain);
    void setWillDelayText(const QString &text);
    void setWillPayloadUtf8(bool enabled);
    void setWillExpiryText(const QString &text);
    void setWillContentType(const QString &contentType);
    void setWillResponseTopic(const QString &topic);
    void setWillCorrelationDataBase64(const QString &data);
    void setWillUserPropertiesText(const QString &text);

    void openForCreate(const SessionConnectionConfig &config);
    void openForEdit(int index, const SessionConnectionConfig &config);
    void loadConfig(const SessionConnectionConfig &config);
    SessionConnectionConfig collectedConfig() const;
    bool validate();

signals:
    void editModeChanged();
    void targetIndexChanged();
    void titleChanged();
    void nameChanged();
    void hostChanged();
    void portTextChanged();
    void connectTimeoutTextChanged();
    void transportChanged();
    void webSocketPathChanged();
    void protocolVersionChanged();
    void sslSecureChanged();
    void alpnChanged();
    void certificateTypeChanged();
    void caFileChanged();
    void clientCertificateFileChanged();
    void clientKeyFileChanged();
    void clientIdChanged();
    void usernameChanged();
    void passwordChanged();
    void keepAliveTextChanged();
    void cleanSessionChanged();
    void sessionExpiryTextChanged();
    void receiveMaximumTextChanged();
    void maximumPacketSizeTextChanged();
    void topicAliasMaximumTextChanged();
    void requestResponseInformationChanged();
    void requestProblemInformationChanged();
    void authenticationMethodChanged();
    void authenticationDataChanged();
    void userPropertiesTextChanged();
    void willEnabledChanged();
    void willTopicChanged();
    void willPayloadChanged();
    void willPayloadFormatChanged();
    void willQosChanged();
    void willRetainChanged();
    void willDelayTextChanged();
    void willPayloadUtf8Changed();
    void willExpiryTextChanged();
    void willContentTypeChanged();
    void willResponseTopicChanged();
    void willCorrelationDataBase64Changed();
    void willUserPropertiesTextChanged();
    void validationErrorChanged();

private:
    QString integerValidationError(const QString &text, const QString &label, quint64 minimum, quint64 maximum, bool required) const;
    void setEditMode(bool editMode);
    void setTargetIndex(int targetIndex);
    void setTitle(const QString &title);
    void setValidationError(const QString &message);

    bool m_editMode = false;
    int m_targetIndex = -1;
    QString m_title = QStringLiteral("New Connection");
    QString m_name;
    QString m_host = QStringLiteral("broker.emqx.io");
    QString m_portText = QStringLiteral("1883");
    QString m_connectTimeoutText = QStringLiteral("10");
    QString m_transport = QStringLiteral("tcp");
    QString m_webSocketPath = QStringLiteral("/mqtt");
    int m_protocolVersion = 5;
    bool m_sslSecure = true;
    QString m_alpn;
    QString m_certificateType = QStringLiteral("ca");
    QString m_caFile;
    QString m_clientCertificateFile;
    QString m_clientKeyFile;
    QString m_clientId;
    QString m_username;
    QString m_password;
    QString m_keepAliveText = QStringLiteral("30");
    bool m_cleanSession = true;
    QString m_sessionExpiryText = QStringLiteral("0");
    QString m_receiveMaximumText;
    QString m_maximumPacketSizeText;
    QString m_topicAliasMaximumText;
    bool m_requestResponseInformation = false;
    bool m_requestProblemInformation = false;
    QString m_authenticationMethod;
    QString m_authenticationData;
    QString m_userPropertiesText;
    bool m_willEnabled = false;
    QString m_willTopic;
    QString m_willPayload;
    int m_willPayloadFormat = 0;
    int m_willQos = 0;
    bool m_willRetain = false;
    QString m_willDelayText;
    bool m_willPayloadUtf8 = false;
    QString m_willExpiryText;
    QString m_willContentType;
    QString m_willResponseTopic;
    QString m_willCorrelationDataBase64;
    QString m_willUserPropertiesText;
    QString m_validationError;
};
