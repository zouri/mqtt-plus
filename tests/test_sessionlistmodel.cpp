#include "domain/session.h"
#include "models/sessionlistmodel.h"

#include <QMqttClient>
#include <QtTest/QtTest>

class SessionListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesConnectionDetails();
    void setSessionsOwnsRowsAndUpdatesWithoutResetWhenCountIsStable();
    void setSessionsResetsWhenCountChanges();
    void viewedTrafficDoesNotInvalidateConnectionRows();
    void unreadTrafficAnnouncesOnlyTheAffectedRowAndRole();
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
    session.runtime.totalMessageCount = 12;
    session.runtime.viewedMessageCount = 8;

    QVector<SessionState> sessions {session};
    SessionListModel model;
    model.setSessions(sessions);

    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, SessionListModel::HostRole).toString(), QStringLiteral("mqtt.example.com"));
    QCOMPARE(model.data(index, SessionListModel::PortRole).toInt(), 1884);
    QCOMPARE(model.data(index, SessionListModel::ClientIdRole).toString(), QStringLiteral("mqtt-id-1"));
    QCOMPARE(model.data(index, SessionListModel::UnreadMessageCountRole).toLongLong(), qint64(4));

    const QVariantMap row = model.rowAt(0);
    QCOMPARE(row.value(QStringLiteral("host")).toString(), QStringLiteral("mqtt.example.com"));
    QCOMPARE(row.value(QStringLiteral("port")).toInt(), 1884);
    QCOMPARE(row.value(QStringLiteral("clientId")).toString(), QStringLiteral("mqtt-id-1"));
    QCOMPARE(row.value(QStringLiteral("unreadMessageCount")).toLongLong(), qint64(4));
}

void SessionListModelTest::setSessionsOwnsRowsAndUpdatesWithoutResetWhenCountIsStable()
{
    SessionState session;
    session.id = QStringLiteral("session-1");
    session.name = QStringLiteral("Production");
    QVector<SessionState> sessions {session};

    SessionListModel model;
    model.setSessions(sessions);

    QSignalSpy dataSpy(&model, &SessionListModel::dataChanged);
    QSignalSpy resetSpy(&model, &SessionListModel::modelReset);
    QSignalSpy countSpy(&model, &SessionListModel::countChanged);

    sessions[0].name = QStringLiteral("Staging");
    QCOMPARE(model.rowAt(0).value(QStringLiteral("name")).toString(), QStringLiteral("Production"));

    model.setSessions(sessions);

    QCOMPARE(model.rowAt(0).value(QStringLiteral("name")).toString(), QStringLiteral("Staging"));
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(dataSpy.first().at(0).toModelIndex().row(), 0);
    QCOMPARE(dataSpy.first().at(1).toModelIndex().row(), 0);
}

void SessionListModelTest::setSessionsResetsWhenCountChanges()
{
    SessionState first;
    first.id = QStringLiteral("session-1");
    first.name = QStringLiteral("Production");
    QVector<SessionState> sessions {first};

    SessionListModel model;
    model.setSessions(sessions);
    QSignalSpy dataSpy(&model, &SessionListModel::dataChanged);
    QSignalSpy resetSpy(&model, &SessionListModel::modelReset);
    QSignalSpy countSpy(&model, &SessionListModel::countChanged);

    SessionState second;
    second.id = QStringLiteral("session-2");
    second.name = QStringLiteral("Staging");
    sessions.append(second);
    model.setSessions(sessions);

    QCOMPARE(model.count(), 2);
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(dataSpy.count(), 0);
}

void SessionListModelTest::viewedTrafficDoesNotInvalidateConnectionRows()
{
    QVector<SessionState> sessions(1);
    sessions[0].id = QStringLiteral("session-1");
    SessionListModel model;
    model.setSessions(sessions);
    QSignalSpy dataSpy(&model, &SessionListModel::dataChanged);

    for (int batch = 1; batch <= 10; ++batch) {
        sessions[0].runtime.totalMessageCount = batch * 10;
        sessions[0].runtime.viewedMessageCount = batch * 10;
        model.setSessions(sessions);
    }
    QCOMPARE(dataSpy.count(), 0);
}

void SessionListModelTest::unreadTrafficAnnouncesOnlyTheAffectedRowAndRole()
{
    QVector<SessionState> sessions(2);
    sessions[0].id = QStringLiteral("session-1");
    sessions[1].id = QStringLiteral("session-2");
    SessionListModel model;
    model.setSessions(sessions);
    QSignalSpy dataSpy(&model, &SessionListModel::dataChanged);
    sessions[1].runtime.totalMessageCount = 10;
    model.setSessions(sessions);

    QCOMPARE(dataSpy.count(), 1);
    QCOMPARE(dataSpy.first().at(0).toModelIndex().row(), 1);
    QCOMPARE(dataSpy.first().at(1).toModelIndex().row(), 1);
    QCOMPARE(dataSpy.first().at(2).value<QList<int>>(),
        QList<int> {SessionListModel::UnreadMessageCountRole});
    QCOMPARE(model.data(model.index(1, 0), SessionListModel::UnreadMessageCountRole).toLongLong(), qint64(10));
}

QTEST_MAIN(SessionListModelTest)

#include "test_sessionlistmodel.moc"
