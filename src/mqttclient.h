#pragma once

#include <QAbstractSocket>
#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QTimer>

class MqttClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(bool useTls READ useTls WRITE setUseTls NOTIFY useTlsChanged)
    Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY clientIdChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(bool cleanSession READ cleanSession WRITE setCleanSession NOTIFY cleanSessionChanged)
    Q_PROPERTY(int keepAliveSeconds READ keepAliveSeconds WRITE setKeepAliveSeconds NOTIFY keepAliveSecondsChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionStateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit MqttClient(QObject *parent = nullptr);
    ~MqttClient() override;

    QString host() const;
    int port() const;
    bool useTls() const;
    QString clientId() const;
    QString username() const;
    QString password() const;
    bool cleanSession() const;
    int keepAliveSeconds() const;
    bool connected() const;
    QString statusText() const;

    void setHost(const QString &host);
    void setPort(int port);
    void setUseTls(bool useTls);
    void setClientId(const QString &clientId);
    void setUsername(const QString &username);
    void setPassword(const QString &password);
    void setCleanSession(bool cleanSession);
    void setKeepAliveSeconds(int keepAliveSeconds);

    Q_INVOKABLE void connectToBroker();
    Q_INVOKABLE void disconnectFromBroker();
    Q_INVOKABLE void subscribe(const QString &topic, int qos = 0);
    Q_INVOKABLE void unsubscribe(const QString &topic);
    Q_INVOKABLE void publish(const QString &topic, const QByteArray &payloadBytes, int qos = 0, bool retain = false);

signals:
    void hostChanged();
    void portChanged();
    void useTlsChanged();
    void clientIdChanged();
    void usernameChanged();
    void passwordChanged();
    void cleanSessionChanged();
    void keepAliveSecondsChanged();
    void connectionStateChanged();
    void statusTextChanged();
    void messageReceived(const QString &topic, const QByteArray &payloadBytes, const QString &timestamp);
    void subscriptionAcknowledged(const QString &topic, int qosGranted);
    void unsubscriptionAcknowledged(const QString &topic);
    void errorOccurred(const QString &message);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onPingTimer();

private:
    static constexpr int kMaxConnectAttempts = 2;

    void setupSocket();
    void resetSocket();
    void openSocketConnection();
    bool shouldRetryConnectError(QAbstractSocket::SocketError socketError) const;
    void setConnected(bool connected);
    void setStatusText(const QString &statusText);
    void markTraffic();
    void sendConnectPacket();
    void sendPacket(quint8 header, const QByteArray &payload);
    void processIncomingPackets();
    void handlePacket(quint8 type, quint8 flags, const QByteArray &packetData);
    void publishError(const QString &message);
    quint16 nextPacketId();

    static QByteArray encodeRemainingLength(int length);
    static bool decodeRemainingLength(const QByteArray &buffer, int start, int &value, int &consumed);
    static QByteArray encodeString(const QString &value);

    QString m_host = QStringLiteral("broker.emqx.io");
    int m_port = 1883;
    bool m_useTls = false;
    QString m_clientId;
    QString m_username;
    QString m_password;
    bool m_cleanSession = true;
    int m_keepAliveSeconds = 30;
    bool m_connected = false;
    QString m_statusText = QStringLiteral("Idle");

    QAbstractSocket *m_socket = nullptr;
    QByteArray m_readBuffer;
    QTimer m_pingTimer;
    QElapsedTimer m_lastTraffic;
    bool m_waitingPingResponse = false;
    quint16 m_packetId = 1;
    int m_connectAttempt = 0;
    QHash<quint16, QString> m_pendingSubscriptions;
    QHash<quint16, QString> m_pendingUnsubscriptions;
};
