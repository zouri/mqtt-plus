#include "topictreemodel.h"

#include <algorithm>
#include <utility>

TopicTreeModel::TopicTreeModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_nodes.append(Node {});
}

int TopicTreeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visibleRows.size();
}

int TopicTreeModel::count() const
{
    return rowCount();
}

QVariant TopicTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleRows.size()) {
        return {};
    }

    const VisibleRow &visibleRow = m_visibleRows.at(index.row());
    const Node &node = m_nodes.at(visibleRow.nodeIndex);
    switch (role) {
    case SegmentRole:
        return node.segment;
    case FullTopicRole:
        return node.fullTopic;
    case DepthRole:
        return visibleRow.depth;
    case TopicRole:
        return node.isTopic;
    case HasChildrenRole:
        return !node.children.isEmpty();
    case ExpandedRole:
        return !node.children.isEmpty()
            && (!m_searchText.isEmpty() || m_expandedNodes.contains(visibleRow.nodeIndex));
    case LastSeenMsRole:
        return node.exactLastSeenMs;
    case LatestPayloadPreviewRole:
        return node.exactPayloadPreview;
    case LatestHistoryIdRole:
        return QString::number(node.exactLatestHistoryId);
    case SubtreeLastSeenMsRole:
        return node.subtreeLastSeenMs;
    default:
        return {};
    }
}

QHash<int, QByteArray> TopicTreeModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {SegmentRole, "segment"},
        {FullTopicRole, "fullTopic"},
        {DepthRole, "depth"},
        {TopicRole, "isTopic"},
        {HasChildrenRole, "hasChildren"},
        {ExpandedRole, "expanded"},
        {LastSeenMsRole, "lastSeenMs"},
        {LatestPayloadPreviewRole, "latestPayloadPreview"},
        {LatestHistoryIdRole, "latestHistoryId"},
        {SubtreeLastSeenMsRole, "subtreeLastSeenMs"},
    };
    return roles;
}

QString TopicTreeModel::searchText() const
{
    return m_searchText;
}

void TopicTreeModel::setSearchText(const QString &searchText)
{
    const QString normalized = searchText.trimmed();
    if (normalized == m_searchText) {
        return;
    }
    m_searchText = normalized;
    emit searchTextChanged();
    rebuildVisibleRows();
}

bool TopicTreeModel::truncated() const
{
    return m_truncated;
}

QVariantMap TopicTreeModel::rowAt(int row) const
{
    if (row < 0 || row >= m_visibleRows.size()) {
        return {};
    }
    return rowToMap(m_visibleRows.at(row));
}

void TopicTreeModel::toggleExpanded(int row)
{
    if (!m_searchText.isEmpty() || row < 0 || row >= m_visibleRows.size()) {
        return;
    }

    const int nodeIndex = m_visibleRows.at(row).nodeIndex;
    if (m_nodes.at(nodeIndex).children.isEmpty()) {
        return;
    }
    if (!m_expandedNodes.remove(nodeIndex)) {
        m_expandedNodes.insert(nodeIndex);
    }
    rebuildVisibleRows();
}

void TopicTreeModel::resetTopics(
    const QString &sourceSessionId,
    const QVector<TopicObservation> &observations)
{
    QVector<TopicObservation> newestFirst = observations;
    std::stable_sort(
        newestFirst.begin(),
        newestFirst.end(),
        [](const TopicObservation &left, const TopicObservation &right) {
            if (left.historyId != right.historyId) {
                return left.historyId > right.historyId;
            }
            return left.observedAtMs > right.observedAtMs;
        });

    QSet<QString> expandedTopics;
    if (sourceSessionId == m_sourceSessionId) {
        for (const int nodeIndex : std::as_const(m_expandedNodes)) {
            if (nodeIndex > 0
                && nodeIndex < m_nodes.size()
                && m_nodes.at(nodeIndex).active) {
                expandedTopics.insert(m_nodes.at(nodeIndex).fullTopic);
            }
        }
    }

    const int previousCount = m_visibleRows.size();
    const bool wasTruncated = m_truncated;
    beginResetModel();
    m_nodes = {Node {}};
    m_freeNodeIndexes.clear();
    m_visibleRows.clear();
    m_topicNodes.clear();
    m_topicsByRecency.clear();
    m_expandedNodes.clear();
    m_sourceSessionId = sourceSessionId;
    m_activeNodeCount = 1;
    m_topicCount = 0;
    m_truncated = false;
    applyObservations(newestFirst);
    for (int nodeIndex = 1; nodeIndex < m_nodes.size(); ++nodeIndex) {
        const Node &node = m_nodes.at(nodeIndex);
        if (node.active && expandedTopics.contains(node.fullTopic)) {
            m_expandedNodes.insert(nodeIndex);
        }
    }
    buildVisibleRows();
    endResetModel();
    if (previousCount != m_visibleRows.size()) {
        emit countChanged();
    }
    if (wasTruncated != m_truncated) {
        emit truncatedChanged();
    }
}

void TopicTreeModel::observeTopics(
    const QString &sourceSessionId,
    const QVector<TopicObservation> &observations)
{
    if (sourceSessionId != m_sourceSessionId || observations.isEmpty()) {
        return;
    }

    const bool wasTruncated = m_truncated;
    const ApplyResult result = applyObservations(observations);
    if (result.structureChanged) {
        rebuildVisibleRows();
    } else if (!result.updatedNodes.isEmpty()) {
        const QList<int> changedRoles {
            LastSeenMsRole,
            LatestPayloadPreviewRole,
            LatestHistoryIdRole,
            SubtreeLastSeenMsRole,
        };
        for (int row = 0; row < m_visibleRows.size(); ++row) {
            if (result.updatedNodes.contains(m_visibleRows.at(row).nodeIndex)) {
                emit dataChanged(index(row, 0), index(row, 0), changedRoles);
            }
        }
    }
    if (wasTruncated != m_truncated) {
        emit truncatedChanged();
    }
}

void TopicTreeModel::clear()
{
    resetTopics({}, {});
}

TopicTreeModel::ApplyResult TopicTreeModel::applyObservations(
    const QVector<TopicObservation> &observations)
{
    ApplyResult result;
    for (const TopicObservation &observation : observations) {
        if (observation.topic.isEmpty()) {
            continue;
        }

        const QStringList segments = observation.topic.split('/', Qt::KeepEmptyParts);
        int parentIndex = 0;
        int firstMissingLevel = -1;
        for (int level = 0; level < segments.size(); ++level) {
            const auto child = m_nodes.at(parentIndex).children.constFind(segments.at(level));
            if (child == m_nodes.at(parentIndex).children.cend()) {
                firstMissingLevel = level;
                break;
            }
            parentIndex = child.value();
        }

        const bool createsTopic = !m_topicNodes.contains(observation.topic);
        const int missingNodeCount = firstMissingLevel < 0
            ? 0
            : segments.size() - firstMissingLevel;
        if (m_activeNodeCount + missingNodeCount > kMaximumNodeCount) {
            m_truncated = true;
            continue;
        }

        if (createsTopic && m_topicCount >= kMaximumTopicCount) {
            m_truncated = true;
            const auto oldestTopic = m_topicsByRecency.cbegin();
            const RecencyKey incomingKey = recencyKey(
                observation.topic,
                observation.historyId,
                observation.observedAtMs);
            if (oldestTopic == m_topicsByRecency.cend()
                || !(oldestTopic.key() < incomingKey)) {
                continue;
            }
            removeTopicNode(oldestTopic.value(), result);
        }

        parentIndex = 0;
        QString fullTopic;
        QVector<int> pathNodes;
        pathNodes.reserve(segments.size());
        for (int level = 0; level < segments.size(); ++level) {
            const QString &segment = segments.at(level);
            fullTopic = level == 0 ? segment : fullTopic + '/' + segment;
            auto child = m_nodes[parentIndex].children.constFind(segment);
            if (child == m_nodes.at(parentIndex).children.cend()) {
                Node node;
                node.segment = segment;
                node.fullTopic = fullTopic;
                node.parentIndex = parentIndex;
                const int childIndex = allocateNode(std::move(node));
                m_nodes[parentIndex].children.insert(segment, childIndex);
                parentIndex = childIndex;
                result.structureChanged = true;
            } else {
                parentIndex = child.value();
            }
            pathNodes.append(parentIndex);
        }

        Node &topicNode = m_nodes[parentIndex];
        const bool wasTopic = topicNode.isTopic;
        if (!wasTopic) {
            topicNode.isTopic = true;
            m_topicNodes.insert(observation.topic, parentIndex);
            ++m_topicCount;
            result.structureChanged = true;
            result.updatedNodes.insert(parentIndex);
        }

        if (observationIsNewer(
                topicNode.exactLatestHistoryId,
                topicNode.exactLastSeenMs,
                observation)) {
            if (wasTopic) {
                m_topicsByRecency.remove(recencyKey(
                    topicNode.fullTopic,
                    topicNode.exactLatestHistoryId,
                    topicNode.exactLastSeenMs));
            }
            topicNode.exactLatestHistoryId = observation.historyId;
            topicNode.exactLastSeenMs = observation.observedAtMs;
            topicNode.exactPayloadPreview = observation.payloadPreview;
            m_topicsByRecency.insert(recencyKey(
                topicNode.fullTopic,
                topicNode.exactLatestHistoryId,
                topicNode.exactLastSeenMs), parentIndex);
            result.updatedNodes.insert(parentIndex);
        }

        for (const int nodeIndex : std::as_const(pathNodes)) {
            Node &node = m_nodes[nodeIndex];
            if (!observationIsNewer(
                    node.subtreeLatestHistoryId,
                    node.subtreeLastSeenMs,
                    observation)) {
                continue;
            }
            node.subtreeLatestHistoryId = observation.historyId;
            node.subtreeLastSeenMs = observation.observedAtMs;
            result.updatedNodes.insert(nodeIndex);
        }
    }
    return result;
}

bool TopicTreeModel::observationIsNewer(
    qint64 historyId,
    qint64 observedAtMs,
    const TopicObservation &observation) const
{
    if (observation.historyId > 0 || historyId > 0) {
        if (observation.historyId != historyId) {
            return observation.historyId > historyId;
        }
        return observation.observedAtMs >= observedAtMs;
    }
    return observation.observedAtMs >= observedAtMs;
}

TopicTreeModel::RecencyKey TopicTreeModel::recencyKey(
    const QString &topic,
    qint64 historyId,
    qint64 observedAtMs) const
{
    return {
        .hasHistoryId = historyId > 0,
        .value = historyId > 0 ? historyId : observedAtMs,
        .observedAtMs = observedAtMs,
        .topic = topic,
    };
}

int TopicTreeModel::allocateNode(Node node)
{
    ++m_activeNodeCount;
    if (m_freeNodeIndexes.isEmpty()) {
        const int nodeIndex = m_nodes.size();
        m_nodes.append(std::move(node));
        return nodeIndex;
    }

    const int nodeIndex = m_freeNodeIndexes.takeLast();
    m_nodes[nodeIndex] = std::move(node);
    return nodeIndex;
}

void TopicTreeModel::removeTopicNode(int nodeIndex, ApplyResult &result)
{
    if (nodeIndex <= 0 || nodeIndex >= m_nodes.size()) {
        return;
    }

    Node &node = m_nodes[nodeIndex];
    if (!node.active || !node.isTopic) {
        return;
    }

    m_topicsByRecency.remove(recencyKey(
        node.fullTopic,
        node.exactLatestHistoryId,
        node.exactLastSeenMs));
    m_topicNodes.remove(node.fullTopic);
    --m_topicCount;
    node.isTopic = false;
    node.exactLatestHistoryId = 0;
    node.exactLastSeenMs = 0;
    node.exactPayloadPreview.clear();
    result.updatedNodes.insert(nodeIndex);
    result.structureChanged = true;
    pruneEmptyBranch(nodeIndex, result);
}

void TopicTreeModel::pruneEmptyBranch(int nodeIndex, ApplyResult &result)
{
    while (nodeIndex > 0) {
        Node &node = m_nodes[nodeIndex];
        if (!node.active || node.isTopic || !node.children.isEmpty()) {
            return;
        }

        const int parentIndex = node.parentIndex;
        m_nodes[parentIndex].children.remove(node.segment);
        m_expandedNodes.remove(nodeIndex);
        node = Node {};
        node.active = false;
        m_freeNodeIndexes.append(nodeIndex);
        --m_activeNodeCount;
        result.structureChanged = true;
        nodeIndex = parentIndex;
    }
}

void TopicTreeModel::rebuildVisibleRows()
{
    const int previousCount = m_visibleRows.size();
    beginResetModel();
    buildVisibleRows();
    endResetModel();
    if (previousCount != m_visibleRows.size()) {
        emit countChanged();
    }
}

void TopicTreeModel::buildVisibleRows()
{
    m_visibleRows.clear();
    QSet<int> searchMatches;
    if (!m_searchText.isEmpty()) {
        collectMatchingSubtrees(0, searchMatches);
    }
    appendVisibleChildren(0, 0, searchMatches);
}

bool TopicTreeModel::collectMatchingSubtrees(int nodeIndex, QSet<int> &matches) const
{
    const Node &node = m_nodes.at(nodeIndex);
    bool matchesSearch = nodeIndex != 0
        && (node.segment.contains(m_searchText, Qt::CaseInsensitive)
            || node.fullTopic.contains(m_searchText, Qt::CaseInsensitive)
            || (node.isTopic
                && node.exactPayloadPreview.contains(
                    m_searchText,
                    Qt::CaseInsensitive)));
    for (const int childIndex : node.children) {
        matchesSearch = collectMatchingSubtrees(childIndex, matches) || matchesSearch;
    }
    if (matchesSearch) {
        matches.insert(nodeIndex);
    }
    return matchesSearch;
}

void TopicTreeModel::appendVisibleChildren(
    int parentIndex,
    int depth,
    const QSet<int> &searchMatches)
{
    const Node &parent = m_nodes.at(parentIndex);
    for (const int childIndex : parent.children) {
        if (!m_searchText.isEmpty() && !searchMatches.contains(childIndex)) {
            continue;
        }
        m_visibleRows.append({childIndex, depth});
        if (!m_searchText.isEmpty() || m_expandedNodes.contains(childIndex)) {
            appendVisibleChildren(childIndex, depth + 1, searchMatches);
        }
    }
}

QVariantMap TopicTreeModel::rowToMap(const VisibleRow &visibleRow) const
{
    const Node &node = m_nodes.at(visibleRow.nodeIndex);
    return {
        {QStringLiteral("segment"), node.segment},
        {QStringLiteral("fullTopic"), node.fullTopic},
        {QStringLiteral("depth"), visibleRow.depth},
        {QStringLiteral("isTopic"), node.isTopic},
        {QStringLiteral("hasChildren"), !node.children.isEmpty()},
        {QStringLiteral("expanded"), !node.children.isEmpty()
                && (!m_searchText.isEmpty() || m_expandedNodes.contains(visibleRow.nodeIndex))},
        {QStringLiteral("lastSeenMs"), node.exactLastSeenMs},
        {QStringLiteral("latestPayloadPreview"), node.exactPayloadPreview},
        {QStringLiteral("latestHistoryId"), QString::number(node.exactLatestHistoryId)},
        {QStringLiteral("subtreeLastSeenMs"), node.subtreeLastSeenMs},
    };
}
