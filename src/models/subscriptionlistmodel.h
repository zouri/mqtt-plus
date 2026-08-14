#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariantList>
#include <QVector>

#include "domain/subscription.h"

class ProcessorLibrary;

class SubscriptionListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role : int {
        TopicRole = Qt::UserRole + 1,
        AliasRole,
        DisplayNameRole,
        RequestedQosRole,
        GrantedQosRole,
        TopicFpsRole,
        TopicRateHistoryRole,
        FormatRole,
        FormatNameRole,
        ProcessorIdRole,
        ProcessorNameRole,
        ProcessorBindingAvailableRole,
        ProcessorBindingDetailRole,
        ColorRole,
        PausedRole,
        StateRole,
        LastErrorRole,
    };
    Q_ENUM(Role)

    explicit SubscriptionListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;

    void setSubscriptions(
        const QString &sourceSessionId,
        const QVector<SubscriptionEntry> &subscriptions,
        const ProcessorLibrary *processorLibrary = nullptr);
    bool updateTopicFps(const QVector<SubscriptionEntry> &subscriptions, qint64 nowMs);

signals:
    void countChanged();

private:
    struct SubscriptionRow
    {
        QString topic;
        QString alias;
        int requestedQos = 0;
        int grantedQos = -1;
        qreal topicFps = 0.0;
        QVariantList topicRateHistory;
        int format = 0;
        QString processorId;
        QString processorName;
        bool processorBindingAvailable = true;
        QString processorBindingDetail;
        QString color;
        bool paused = false;
        QString state;
        QString lastError;
    };

    static SubscriptionRow rowFromSubscription(
        const SubscriptionEntry &subscription,
        const ProcessorLibrary *processorLibrary,
        qint64 nowMs);
    static QVariantMap rowToMap(const SubscriptionRow &row);
    static QString displayName(const SubscriptionRow &row);
    static QList<int> changedRoles(
        const SubscriptionRow &before,
        const SubscriptionRow &after);

    QVector<SubscriptionRow> m_rows;
    QString m_sourceSessionId;
    qint64 m_lastRateHistorySampleMs = 0;
};
