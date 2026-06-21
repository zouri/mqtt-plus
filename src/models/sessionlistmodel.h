#pragma once

#include <QAbstractListModel>
#include <QVector>

struct SessionListRow {
    QString id;
    QString name;
    QString state;
    bool connected = false;
    QString host;
    int port = 0;
    QString transport;
    QString transportLabel;
    int protocolVersion = 5;
    QString protocolVersionName;
    QString summary;
    QString lastError;
};

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
        TransportRole,
        TransportLabelRole,
        ProtocolVersionRole,
        ProtocolVersionNameRole,
        SummaryRole,
        LastErrorRole,
    };
    Q_ENUM(Role)

    explicit SessionListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;

    void setRows(const QVector<SessionListRow> &rows);

signals:
    void countChanged();

private:
    QVariantMap rowToMap(const SessionListRow &row) const;

    QVector<SessionListRow> m_rows;
};
