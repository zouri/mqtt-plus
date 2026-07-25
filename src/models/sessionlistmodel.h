#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

struct SessionState;

class SessionListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role : int {
        IdRole = Qt::UserRole + 1,
        NameRole,
        StateRole,
        ConnectedRole,
        HostRole,
        PortRole,
        ClientIdRole,
        TransportRole,
        TransportLabelRole,
        ProtocolVersionRole,
        ProtocolVersionNameRole,
        SummaryRole,
        LastErrorRole,
        UnreadMessageCountRole,
    };
    Q_ENUM(Role)

    explicit SessionListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;

    void setSessions(const QVector<SessionState> &sessions);

signals:
    void countChanged();

private:
    struct SessionRow
    {
        QString id;
        QString name;
        QString state;
        bool connected = false;
        QString host;
        int port = 0;
        QString clientId;
        QString transport;
        QString transportLabel;
        int protocolVersion = 0;
        QString protocolVersionName;
        QString summary;
        QString lastError;
        qint64 unreadMessageCount = 0;
    };

    static SessionRow rowFromSession(const SessionState &session);

    QVector<SessionRow> m_rows;
};
