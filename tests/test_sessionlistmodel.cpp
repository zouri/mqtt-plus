#include "domain/session.h"
#include "models/sessionlistmodel.h"

#include <QMqttClient>
#include <QtTest/QtTest>

class SessionListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesConnectionDetails();
    void notifyRefreshUpdatesRowsWithoutResetWhenCountIsStable();
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
    session.runtime.client = &client;

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

void SessionListModelTest::notifyRefreshUpdatesRowsWithoutResetWhenCountIsStable()
{
    SessionState session;
    session.id = QStringLiteral("session-1");
    session.name = QStringLiteral("Production");
    QVector<SessionState> sessions {session};

    SessionListModel model;
    model.setSource(&sessions);

    QSignalSpy dataSpy(&model, &SessionListModel::dataChanged);
    QSignalSpy resetSpy(&model, &SessionListModel::modelReset);
    QSignalSpy countSpy(&model, &SessionListModel::countChanged);

    sessions[0].name = QStringLiteral("Staging");
    model.notifyRefresh();

    QCOMPARE(model.rowAt(0).value(QStringLiteral("name")).toString(), QStringLiteral("Staging"));
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(dataSpy.first().at(0).toModelIndex().row(), 0);
    QCOMPARE(dataSpy.first().at(1).toModelIndex().row(), 0);
}

QTEST_MAIN(SessionListModelTest)

#include "test_sessionlistmodel.moc"
