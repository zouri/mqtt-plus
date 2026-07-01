#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "domain/subscription.h"

#include <functional>

struct SessionState;

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
        FormatRole,
        FormatNameRole,
        ScriptIdRole,
        ScriptNameRole,
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

    void setSource(const SessionState *session);
    void setScriptNameLookup(std::function<QString(const QString &)> lookup);
    void notifyRefresh();
    void updateTopicFps(qint64 nowMs);

signals:
    void countChanged();

private:
    QVariantMap rowToMap(const SubscriptionEntry &sub, int row) const;
    void rebuildCache();
    QString displayNameForSub(const SubscriptionEntry &sub) const;

    const QVector<SubscriptionEntry> *m_subs = nullptr;
    QVector<SubscriptionEntry> m_empty;
    QVector<qreal> m_fpsCache;
    QVector<QString> m_scriptNameCache;
    std::function<QString(const QString &)> m_scriptNameLookup;
};
