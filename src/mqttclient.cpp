#include "mqttclient.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QSslSocket>
#include <QTcpSocket>

namespace {
constexpr quint8 kConnectPacketType = 0x10;
constexpr quint8 kPublishPacketBase = 0x30;
constexpr quint8 kSubscribePacketType = 0x82;
constexpr quint8 kUnsubscribePacketType = 0xA2;
constexpr quint8 kPingReqPacketType = 0xC0;
constexpr quint8 kDisconnectPacketType = 0xE0;
constexpr quint8 kPubAckPacketType = 0x40;
constexpr quint8 kProtocolLevel311 = 0x04;

constexpr quint8 kConnAckType = 0x02;
constexpr quint8 kPublishType = 0x03;
constexpr quint8 kSubAckType = 0x09;
constexpr quint8 kUnsubAckType = 0x0B;
constexpr quint8 kPingRespType = 0x0D;

QString socketErrorName(QAbstractSocket::SocketError error)
{
    switch (error) {
    case QAbstractSocket::ConnectionRefusedError:
        return QStringLiteral("ConnectionRefusedError");
    case QAbstractSocket::RemoteHostClosedError:
        return QStringLiteral("RemoteHostClosedError");
    case QAbstractSocket::HostNotFoundError:
        return QStringLiteral("HostNotFoundError");
    case QAbstractSocket::SocketAccessError:
        return QStringLiteral("SocketAccessError");
    case QAbstractSocket::SocketResourceError:
        return QStringLiteral("SocketResourceError");
    case QAbstractSocket::SocketTimeoutError:
        return QStringLiteral("SocketTimeoutError");
    case QAbstractSocket::DatagramTooLargeError:
        return QStringLiteral("DatagramTooLargeError");
    case QAbstractSocket::NetworkError:
        return QStringLiteral("NetworkError");
    case QAbstractSocket::AddressInUseError:
        return QStringLiteral("AddressInUseError");
    case QAbstractSocket::SocketAddressNotAvailableError:
        return QStringLiteral("SocketAddressNotAvailableError");
    case QAbstractSocket::UnsupportedSocketOperationError:
        return QStringLiteral("UnsupportedSocketOperationError");
    case QAbstractSocket::UnfinishedSocketOperationError:
        return QStringLiteral("UnfinishedSocketOperationError");
    case QAbstractSocket::ProxyAuthenticationRequiredError:
        return QStringLiteral("ProxyAuthenticationRequiredError");
    case QAbstractSocket::SslHandshakeFailedError:
        return QStringLiteral("SslHandshakeFailedError");
    case QAbstractSocket::ProxyConnectionRefusedError:
        return QStringLiteral("ProxyConnectionRefusedError");
    case QAbstractSocket::ProxyConnectionClosedError:
        return QStringLiteral("ProxyConnectionClosedError");
    case QAbstractSocket::ProxyConnectionTimeoutError:
        return QStringLiteral("ProxyConnectionTimeoutError");
    case QAbstractSocket::ProxyNotFoundError:
        return QStringLiteral("ProxyNotFoundError");
    case QAbstractSocket::ProxyProtocolError:
        return QStringLiteral("ProxyProtocolError");
    case QAbstractSocket::OperationError:
        return QStringLiteral("OperationError");
    case QAbstractSocket::SslInternalError:
        return QStringLiteral("SslInternalError");
    case QAbstractSocket::SslInvalidUserDataError:
        return QStringLiteral("SslInvalidUserDataError");
    case QAbstractSocket::TemporaryError:
        return QStringLiteral("TemporaryError");
    case QAbstractSocket::UnknownSocketError:
        return QStringLiteral("UnknownSocketError");
    default:
        return QStringLiteral("SocketError(%1)").arg(static_cast<int>(error));
    }
}
}

MqttClient::MqttClient(QObject *parent)
    : QObject(parent)
{
    m_clientId = QStringLiteral("mqtt-plus-%1")
                     .arg(QRandomGenerator::global()->bounded(100000, 999999));

    m_pingTimer.setSingleShot(false);
    connect(&m_pingTimer, &QTimer::timeout, this, &MqttClient::onPingTimer);
}

MqttClient::~MqttClient()
{
    resetSocket();
}

QString MqttClient::host() const
{
    return m_host;
}

int MqttClient::port() const
{
    return m_port;
}

bool MqttClient::useTls() const
{
    return m_useTls;
}

QString MqttClient::clientId() const
{
    return m_clientId;
}

QString MqttClient::username() const
{
    return m_username;
}

QString MqttClient::password() const
{
    return m_password;
}

bool MqttClient::cleanSession() const
{
    return m_cleanSession;
}

int MqttClient::keepAliveSeconds() const
{
    return m_keepAliveSeconds;
}

bool MqttClient::connected() const
{
    return m_connected;
}

QString MqttClient::statusText() const
{
    return m_statusText;
}

void MqttClient::setHost(const QString &host)
{
    if (m_host == host) {
        return;
    }
    m_host = host.trimmed();
    emit hostChanged();
}

void MqttClient::setPort(int port)
{
    if (m_port == port) {
        return;
    }
    m_port = qBound(1, port, 65535);
    emit portChanged();
}

void MqttClient::setUseTls(bool useTls)
{
    if (m_useTls == useTls) {
        return;
    }
    m_useTls = useTls;
    emit useTlsChanged();
}

void MqttClient::setClientId(const QString &clientId)
{
    if (m_clientId == clientId) {
        return;
    }
    m_clientId = clientId.trimmed();
    emit clientIdChanged();
}

void MqttClient::setUsername(const QString &username)
{
    if (m_username == username) {
        return;
    }
    m_username = username;
    emit usernameChanged();
}

void MqttClient::setPassword(const QString &password)
{
    if (m_password == password) {
        return;
    }
    m_password = password;
    emit passwordChanged();
}

void MqttClient::setCleanSession(bool cleanSession)
{
    if (m_cleanSession == cleanSession) {
        return;
    }
    m_cleanSession = cleanSession;
    emit cleanSessionChanged();
}

void MqttClient::setKeepAliveSeconds(int keepAliveSeconds)
{
    const int sanitized = qBound(5, keepAliveSeconds, 1200);
    if (m_keepAliveSeconds == sanitized) {
        return;
    }
    m_keepAliveSeconds = sanitized;
    emit keepAliveSecondsChanged();
}

void MqttClient::connectToBroker()
{
    if (m_host.isEmpty()) {
        publishError(QStringLiteral("Broker host cannot be empty."));
        return;
    }
    if (m_clientId.isEmpty()) {
        publishError(QStringLiteral("Client ID cannot be empty."));
        return;
    }
    if (!m_password.isEmpty() && m_username.isEmpty()) {
        publishError(QStringLiteral("Username is required when password is set."));
        return;
    }

    resetSocket();
    setupSocket();

    if (!m_socket) {
        publishError(QStringLiteral("Unable to create socket."));
        return;
    }

    m_connectAttempt = 1;
    m_readBuffer.clear();
    m_pendingSubscriptions.clear();
    m_pendingUnsubscriptions.clear();
    m_waitingPingResponse = false;
    openSocketConnection();
}

void MqttClient::disconnectFromBroker()
{
    m_connectAttempt = 0;
    if (!m_socket) {
        setConnected(false);
        setStatusText(QStringLiteral("Disconnected"));
        return;
    }

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        sendPacket(kDisconnectPacketType, {});
    }
    m_socket->disconnectFromHost();
}

void MqttClient::subscribe(const QString &topic, int qos)
{
    if (!m_connected || !m_socket) {
        publishError(QStringLiteral("Not connected."));
        return;
    }

    const QString trimmed = topic.trimmed();
    if (trimmed.isEmpty()) {
        publishError(QStringLiteral("Topic cannot be empty."));
        return;
    }

    const int requestedQos = qBound(0, qos, 1);
    const quint16 packetId = nextPacketId();

    QByteArray payload;
    payload.reserve(64);
    payload.append(static_cast<char>((packetId >> 8) & 0xFF));
    payload.append(static_cast<char>(packetId & 0xFF));
    payload.append(encodeString(trimmed));
    payload.append(static_cast<char>(requestedQos & 0x03));

    m_pendingSubscriptions.insert(packetId, trimmed);
    sendPacket(kSubscribePacketType, payload);
}

void MqttClient::publish(const QString &topic, const QByteArray &payloadBytes, int qos, bool retain)
{
    if (!m_connected || !m_socket) {
        publishError(QStringLiteral("Not connected."));
        return;
    }

    const QString trimmed = topic.trimmed();
    if (trimmed.isEmpty()) {
        publishError(QStringLiteral("Topic cannot be empty."));
        return;
    }

    const int requestedQos = qBound(0, qos, 1);
    if (requestedQos > 0) {
        publishError(QStringLiteral("Outgoing QoS>0 is not implemented yet. Sending with QoS0."));
    }

    quint8 header = kPublishPacketBase;
    if (retain) {
        header |= 0x01;
    }

    QByteArray body;
    body.reserve(128);
    body.append(encodeString(trimmed));
    body.append(payloadBytes);
    sendPacket(header, body);
}

void MqttClient::unsubscribe(const QString &topic)
{
    if (!m_connected || !m_socket) {
        publishError(QStringLiteral("Not connected."));
        return;
    }

    const QString trimmed = topic.trimmed();
    if (trimmed.isEmpty()) {
        publishError(QStringLiteral("Topic cannot be empty."));
        return;
    }

    const quint16 packetId = nextPacketId();

    QByteArray payload;
    payload.reserve(64);
    payload.append(static_cast<char>((packetId >> 8) & 0xFF));
    payload.append(static_cast<char>(packetId & 0xFF));
    payload.append(encodeString(trimmed));

    m_pendingUnsubscriptions.insert(packetId, trimmed);
    sendPacket(kUnsubscribePacketType, payload);
}

void MqttClient::onConnected()
{
    setStatusText(QStringLiteral("Socket connected, waiting for CONNACK ..."));
    sendConnectPacket();
}

void MqttClient::onDisconnected()
{
    setConnected(false);
    m_connectAttempt = 0;
    setStatusText(QStringLiteral("Disconnected"));
    m_pingTimer.stop();
    m_waitingPingResponse = false;
}

void MqttClient::onReadyRead()
{
    if (!m_socket) {
        return;
    }
    m_readBuffer.append(m_socket->readAll());
    markTraffic();
    processIncomingPackets();
}

void MqttClient::onSocketError(QAbstractSocket::SocketError socketError)
{
    if (!m_socket) {
        return;
    }

    const QString diagnostic = QStringLiteral(
                                   "Socket diag: code=%1(%2), text=%3, host=%4, port=%5, tls=%6, "
                                   "attempt=%7/%8, state=%9")
                                   .arg(static_cast<int>(socketError))
                                   .arg(socketErrorName(socketError))
                                   .arg(m_socket->errorString())
                                   .arg(m_host)
                                   .arg(m_port)
                                   .arg(m_useTls ? QStringLiteral("on") : QStringLiteral("off"))
                                   .arg(qMax(1, m_connectAttempt))
                                   .arg(kMaxConnectAttempts)
                                   .arg(static_cast<int>(m_socket->state()));
    publishError(diagnostic);

    const QString errorText = m_socket->errorString();
    if (!m_connected && m_connectAttempt > 0 && m_connectAttempt < kMaxConnectAttempts
        && shouldRetryConnectError(socketError)) {
        ++m_connectAttempt;
        setStatusText(
            QStringLiteral("Connect failed (%1), retrying %2/%3 ...")
                .arg(errorText)
                .arg(m_connectAttempt)
                .arg(kMaxConnectAttempts));
        resetSocket();
        setupSocket();
        openSocketConnection();
        return;
    }

    m_connectAttempt = 0;
    publishError(QStringLiteral("Socket error: %1").arg(errorText));
    setConnected(false);
    m_pingTimer.stop();
}

void MqttClient::onPingTimer()
{
    if (!m_connected || !m_socket) {
        return;
    }
    if (!m_lastTraffic.isValid()) {
        markTraffic();
    }

    const qint64 idleMs = m_lastTraffic.elapsed();
    const int threshold = qMax(1000, (m_keepAliveSeconds * 1000) / 2);
    if (idleMs < threshold) {
        return;
    }
    if (m_waitingPingResponse) {
        publishError(QStringLiteral("Ping timeout, disconnecting."));
        m_socket->disconnectFromHost();
        return;
    }

    sendPacket(kPingReqPacketType, {});
    m_waitingPingResponse = true;
}

void MqttClient::setupSocket()
{
    if (m_useTls) {
        auto *sslSocket = new QSslSocket(this);
        connect(sslSocket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
            if (!errors.isEmpty()) {
                publishError(QStringLiteral("TLS error: %1").arg(errors.constFirst().errorString()));
            }
        });
        m_socket = sslSocket;
    } else {
        m_socket = new QTcpSocket(this);
    }

    connect(m_socket, &QAbstractSocket::connected, this, &MqttClient::onConnected);
    connect(m_socket, &QAbstractSocket::disconnected, this, &MqttClient::onDisconnected);
    connect(m_socket, &QIODevice::readyRead, this, &MqttClient::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &MqttClient::onSocketError);
}

void MqttClient::openSocketConnection()
{
    if (!m_socket) {
        return;
    }

    QString status = QStringLiteral("Connecting to %1:%2 ...").arg(m_host).arg(m_port);
    if (m_connectAttempt > 1) {
        status = QStringLiteral("%1 (%2/%3)")
                     .arg(status)
                     .arg(m_connectAttempt)
                     .arg(kMaxConnectAttempts);
    }
    setStatusText(status);

    if (m_useTls) {
        QSslSocket *sslSocket = qobject_cast<QSslSocket *>(m_socket);
        if (!sslSocket) {
            publishError(QStringLiteral("TLS socket initialization failed."));
            m_connectAttempt = 0;
            return;
        }
        sslSocket->connectToHostEncrypted(m_host, static_cast<quint16>(m_port));
        return;
    }

    m_socket->connectToHost(m_host, static_cast<quint16>(m_port));
}

bool MqttClient::shouldRetryConnectError(QAbstractSocket::SocketError socketError) const
{
    switch (socketError) {
    case QAbstractSocket::HostNotFoundError:
    case QAbstractSocket::NetworkError:
    case QAbstractSocket::SocketTimeoutError:
    case QAbstractSocket::TemporaryError:
        return true;
    default:
        return false;
    }
}

void MqttClient::resetSocket()
{
    if (!m_socket) {
        return;
    }
    m_pingTimer.stop();
    m_waitingPingResponse = false;
    disconnect(m_socket, nullptr, this, nullptr);
    m_socket->abort();
    m_socket->deleteLater();
    m_socket = nullptr;
}

void MqttClient::setConnected(bool connected)
{
    if (m_connected == connected) {
        return;
    }
    m_connected = connected;
    emit connectionStateChanged();
}

void MqttClient::setStatusText(const QString &statusText)
{
    if (m_statusText == statusText) {
        return;
    }
    m_statusText = statusText;
    emit statusTextChanged();
}

void MqttClient::markTraffic()
{
    m_lastTraffic.restart();
}

void MqttClient::sendConnectPacket()
{
    QByteArray payload;
    payload.reserve(128);

    payload.append(encodeString(QStringLiteral("MQTT")));
    payload.append(static_cast<char>(kProtocolLevel311));

    quint8 connectFlags = 0;
    if (!m_username.isEmpty()) {
        connectFlags |= 0x80;
    }
    if (!m_password.isEmpty()) {
        connectFlags |= 0x40;
    }
    if (m_cleanSession) {
        connectFlags |= 0x02;
    }
    payload.append(static_cast<char>(connectFlags));
    payload.append(static_cast<char>((m_keepAliveSeconds >> 8) & 0xFF));
    payload.append(static_cast<char>(m_keepAliveSeconds & 0xFF));

    payload.append(encodeString(m_clientId));
    if (!m_username.isEmpty()) {
        payload.append(encodeString(m_username));
    }
    if (!m_password.isEmpty()) {
        payload.append(encodeString(m_password));
    }

    sendPacket(kConnectPacketType, payload);
}

void MqttClient::sendPacket(quint8 header, const QByteArray &payload)
{
    if (!m_socket) {
        return;
    }

    QByteArray packet;
    packet.reserve(payload.size() + 6);
    packet.append(static_cast<char>(header));
    packet.append(encodeRemainingLength(payload.size()));
    packet.append(payload);
    m_socket->write(packet);
    markTraffic();
}

void MqttClient::processIncomingPackets()
{
    while (true) {
        if (m_readBuffer.size() < 2) {
            return;
        }

        const quint8 firstByte = static_cast<quint8>(m_readBuffer.at(0));
        int remainingLength = 0;
        int lengthBytes = 0;
        if (!decodeRemainingLength(m_readBuffer, 1, remainingLength, lengthBytes)) {
            return;
        }

        const int packetHeaderSize = 1 + lengthBytes;
        const int totalPacketSize = packetHeaderSize + remainingLength;
        if (m_readBuffer.size() < totalPacketSize) {
            return;
        }

        const QByteArray packetData = m_readBuffer.mid(packetHeaderSize, remainingLength);
        const quint8 packetType = firstByte >> 4;
        const quint8 packetFlags = firstByte & 0x0F;
        handlePacket(packetType, packetFlags, packetData);

        m_readBuffer.remove(0, totalPacketSize);
    }
}

void MqttClient::handlePacket(quint8 type, quint8 flags, const QByteArray &packetData)
{
    switch (type) {
    case kConnAckType: {
        if (packetData.size() < 2) {
            publishError(QStringLiteral("Invalid CONNACK packet."));
            return;
        }
        const quint8 code = static_cast<quint8>(packetData.at(1));
        if (code == 0) {
            setConnected(true);
            m_connectAttempt = 0;
            setStatusText(QStringLiteral("Connected to %1:%2").arg(m_host).arg(m_port));
            const int intervalMs = qMax(1000, (m_keepAliveSeconds * 1000) / 2);
            m_pingTimer.start(intervalMs);
            markTraffic();
        } else {
            setConnected(false);
            m_connectAttempt = 0;
            setStatusText(QStringLiteral("Broker rejected connection, code=%1").arg(code));
            if (m_socket) {
                m_socket->disconnectFromHost();
            }
        }
        return;
    }

    case kPublishType: {
        if (packetData.size() < 2) {
            return;
        }
        int offset = 0;
        const int topicLength =
            (static_cast<quint8>(packetData.at(offset)) << 8) |
            static_cast<quint8>(packetData.at(offset + 1));
        offset += 2;

        if (packetData.size() < offset + topicLength) {
            return;
        }
        const QString topic = QString::fromUtf8(packetData.mid(offset, topicLength));
        offset += topicLength;

        const int qos = (flags >> 1) & 0x03;
        quint16 packetId = 0;
        if (qos > 0) {
            if (packetData.size() < offset + 2) {
                return;
            }
            packetId = (static_cast<quint8>(packetData.at(offset)) << 8) |
                       static_cast<quint8>(packetData.at(offset + 1));
            offset += 2;
        }

        const QByteArray payloadBytes = packetData.mid(offset);
        emit messageReceived(topic, payloadBytes, QDateTime::currentDateTime().toString(Qt::ISODateWithMs));

        if (qos == 1) {
            QByteArray ack;
            ack.append(static_cast<char>((packetId >> 8) & 0xFF));
            ack.append(static_cast<char>(packetId & 0xFF));
            sendPacket(kPubAckPacketType, ack);
        }
        return;
    }

    case kSubAckType: {
        if (packetData.size() < 3) {
            return;
        }
        const quint16 packetId =
            (static_cast<quint8>(packetData.at(0)) << 8) |
            static_cast<quint8>(packetData.at(1));
        const int grantedQos = static_cast<quint8>(packetData.at(2));
        const QString topic = m_pendingSubscriptions.take(packetId);
        if (!topic.isEmpty()) {
            emit subscriptionAcknowledged(topic, grantedQos);
        }
        return;
    }

    case kUnsubAckType: {
        if (packetData.size() < 2) {
            return;
        }
        const quint16 packetId =
            (static_cast<quint8>(packetData.at(0)) << 8) |
            static_cast<quint8>(packetData.at(1));
        const QString topic = m_pendingUnsubscriptions.take(packetId);
        if (!topic.isEmpty()) {
            emit unsubscriptionAcknowledged(topic);
        }
        return;
    }

    case kPingRespType:
        m_waitingPingResponse = false;
        markTraffic();
        return;

    default:
        return;
    }
}

void MqttClient::publishError(const QString &message)
{
    setStatusText(message);
    emit errorOccurred(message);
}

quint16 MqttClient::nextPacketId()
{
    const quint16 id = m_packetId;
    ++m_packetId;
    if (m_packetId == 0) {
        m_packetId = 1;
    }
    return id;
}

QByteArray MqttClient::encodeRemainingLength(int length)
{
    QByteArray encoded;
    int value = length;
    do {
        quint8 byte = static_cast<quint8>(value % 128);
        value /= 128;
        if (value > 0) {
            byte |= 0x80;
        }
        encoded.append(static_cast<char>(byte));
    } while (value > 0);
    return encoded;
}

bool MqttClient::decodeRemainingLength(const QByteArray &buffer, int start, int &value, int &consumed)
{
    value = 0;
    consumed = 0;
    int multiplier = 1;

    for (int i = start; i < buffer.size(); ++i) {
        const quint8 byte = static_cast<quint8>(buffer.at(i));
        value += (byte & 0x7F) * multiplier;
        ++consumed;

        if ((byte & 0x80) == 0) {
            return true;
        }

        multiplier *= 128;
        if (multiplier > 128 * 128 * 128) {
            return false;
        }
    }

    return false;
}

QByteArray MqttClient::encodeString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    QByteArray encoded;
    encoded.reserve(utf8.size() + 2);
    encoded.append(static_cast<char>((utf8.size() >> 8) & 0xFF));
    encoded.append(static_cast<char>(utf8.size() & 0xFF));
    encoded.append(utf8);
    return encoded;
}
