#pragma once

#include <QAbstractListModel>
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
    };
    Q_ENUM(Role)

    explicit SessionListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;

    void setSource(const QVector<SessionState> *sessions);
    void notifyRefresh();

signals:
    void countChanged();

private:
    const QVector<SessionState> *m_sessions = nullptr;
    int m_knownCount = 0;
};
