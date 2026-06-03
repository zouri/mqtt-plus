#pragma once

#include <QAbstractListModel>
#include <QVector>

struct SubscriptionListRow {
    QString topic;
    QString alias;
    QString displayName;
    int requestedQos = 0;
    int grantedQos = -1;
    qreal topicFps = 0.0;
    int format = 0;
    QString formatName;
    QString scriptId;
    QString scriptName;
    bool paused = false;
    QString state;
    QString lastError;
};

class SubscriptionListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        TopicRole = Qt::UserRole + 1,
        AliasRole,
        DisplayNameRole,
        RequestedQosRole,
        GrantedQosRole,
        TopicFpsRole,
        FormatRole,
        FormatNameRole,
        ScriptIdRole,
        ScriptNameRole,
        PausedRole,
        StateRole,
        LastErrorRole
    };
    Q_ENUM(Role)

    explicit SubscriptionListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;

    void setRows(const QVector<SubscriptionListRow> &rows);

signals:
    void countChanged();

private:
    QVariantMap rowToMap(const SubscriptionListRow &row) const;

    QVector<SubscriptionListRow> m_rows;
};
