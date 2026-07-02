#include "domain/session.h"
#include "models/sessionlistmodel.h"

#include <QMqttClient>
#include <QtTest/QtTest>

class SessionListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesConnectionDetails();
};

void SessionListModelTest::exposesConnectionDetails()
{
    QMqttClient client;
    client.setHostname(QStringLiteral("mqtt.example.com"));
    client.setPort(1884);
    client.setClientId(QStringLiteral("mqtt-id-1"));

    SessionState session;
    session.id = QStringLiteral("session-1");
    session.name = QStringLiteral("Production");
    session.client = &client;

    QVector<SessionState> sessions {session};
    SessionListModel model;
    model.setSource(&sessions);

    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, SessionListModel::HostRole).toString(), QStringLiteral("mqtt.example.com"));
    QCOMPARE(model.data(index, SessionListModel::PortRole).toInt(), 1884);
    QCOMPARE(model.data(index, SessionListModel::ClientIdRole).toString(), QStringLiteral("mqtt-id-1"));

    const QVariantMap row = model.rowAt(0);
    QCOMPARE(row.value(QStringLiteral("host")).toString(), QStringLiteral("mqtt.example.com"));
    QCOMPARE(row.value(QStringLiteral("port")).toInt(), 1884);
    QCOMPARE(row.value(QStringLiteral("clientId")).toString(), QStringLiteral("mqtt-id-1"));
}

QTEST_MAIN(SessionListModelTest)

#include "test_sessionlistmodel.moc"
