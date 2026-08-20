#pragma once

#include "domain/topicobservation.h"

#include <QAbstractListModel>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <tuple>

class TopicTreeModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(bool truncated READ truncated NOTIFY truncatedChanged)

public:
    enum Role : int {
        SegmentRole = Qt::UserRole + 1,
        FullTopicRole,
        DepthRole,
        TopicRole,
        HasChildrenRole,
        ExpandedRole,
        LastSeenMsRole,
        LatestPayloadPreviewRole,
        LatestHistoryIdRole,
        SubtreeLastSeenMsRole,
    };
    Q_ENUM(Role)

    explicit TopicTreeModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString searchText() const;
    void setSearchText(const QString &searchText);
    bool truncated() const;

    Q_INVOKABLE QVariantMap rowAt(int row) const;
    Q_INVOKABLE void toggleExpanded(int row);

    void resetTopics(
        const QString &sourceSessionId,
        const QVector<TopicObservation> &observations);
    void observeTopics(
        const QString &sourceSessionId,
        const QVector<TopicObservation> &observations);
    void clear();

signals:
    void countChanged();
    void searchTextChanged();
    void truncatedChanged();

private:
    static constexpr int kMaximumTopicCount = 10'000;
    static constexpr int kMaximumNodeCount = 50'000;

    struct Node
    {
        QString segment;
        QString fullTopic;
        QMap<QString, int> children;
        int parentIndex = -1;
        bool active = true;
        bool isTopic = false;
        qint64 exactLatestHistoryId = 0;
        qint64 exactLastSeenMs = 0;
        QString exactPayloadPreview;
        qint64 subtreeLatestHistoryId = 0;
        qint64 subtreeLastSeenMs = 0;
    };

    struct RecencyKey
    {
        bool hasHistoryId = false;
        qint64 value = 0;
        qint64 observedAtMs = 0;
        QString topic;

        friend bool operator<(const RecencyKey &left, const RecencyKey &right)
        {
            return std::tie(
                left.hasHistoryId,
                left.value,
                left.observedAtMs,
                left.topic)
                < std::tie(
                    right.hasHistoryId,
                    right.value,
                    right.observedAtMs,
                    right.topic);
        }
    };

    struct VisibleRow
    {
        int nodeIndex = 0;
        int depth = 0;
    };

    struct ApplyResult
    {
        QSet<int> updatedNodes;
        bool structureChanged = false;
    };

    ApplyResult applyObservations(const QVector<TopicObservation> &observations);
    bool observationIsNewer(
        qint64 historyId,
        qint64 observedAtMs,
        const TopicObservation &observation) const;
    RecencyKey recencyKey(
        const QString &topic,
        qint64 historyId,
        qint64 observedAtMs) const;
    int allocateNode(Node node);
    void removeTopicNode(int nodeIndex, ApplyResult &result);
    void pruneEmptyBranch(int nodeIndex, ApplyResult &result);
    void rebuildVisibleRows();
    void buildVisibleRows();
    bool collectMatchingSubtrees(int nodeIndex, QSet<int> &matches) const;
    void appendVisibleChildren(
        int parentIndex,
        int depth,
        const QSet<int> &searchMatches);
    QVariantMap rowToMap(const VisibleRow &row) const;

    QVector<Node> m_nodes;
    QVector<int> m_freeNodeIndexes;
    QVector<VisibleRow> m_visibleRows;
    QHash<QString, int> m_topicNodes;
    QMap<RecencyKey, int> m_topicsByRecency;
    QSet<int> m_expandedNodes;
    QString m_sourceSessionId;
    QString m_searchText;
    int m_activeNodeCount = 1;
    int m_topicCount = 0;
    bool m_truncated = false;
};
