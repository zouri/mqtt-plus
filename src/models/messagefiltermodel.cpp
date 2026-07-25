#include "messagefiltermodel.h"

#include "models/eventstreammodel.h"
#include "services/payload/payloadcodec.h"

MessageFilterModel::MessageFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    connectCountSignals();
    connect(this,
            &QAbstractProxyModel::sourceModelChanged,
            this,
            &MessageFilterModel::connectSourceSignals);
    connect(this,
            &QAbstractProxyModel::sourceModelChanged,
            this,
            &MessageFilterModel::messageCountsChanged);
}

QString MessageFilterModel::filterText() const
{
    return m_filterText;
}

QStringList MessageFilterModel::selectedTopics() const
{
    return m_selectedTopics;
}

QString MessageFilterModel::direction() const
{
    return m_direction;
}

bool MessageFilterModel::filterActive() const
{
    return !m_filterText.isEmpty() || !m_selectedTopics.isEmpty() || m_direction != QStringLiteral("all");
}

int MessageFilterModel::count() const
{
    return rowCount();
}

int MessageFilterModel::filteredMessageCount() const
{
    return messageCount(this);
}

int MessageFilterModel::totalMessageCount() const
{
    return messageCount(sourceModel());
}

void MessageFilterModel::setFilterText(const QString &filterText)
{
    const bool wasActive = filterActive();
    const QString normalized = filterText.trimmed();
    if (m_filterText == normalized) {
        return;
    }
    m_filterText = normalized;
    invalidateRows(wasActive);
    emit filterTextChanged();
}

void MessageFilterModel::setSelectedTopics(const QStringList &selectedTopics)
{
    const bool wasActive = filterActive();
    QStringList normalized;
    for (const QString &topic : selectedTopics) {
        const QString trimmed = topic.trimmed();
        if (!trimmed.isEmpty() && !normalized.contains(trimmed)) {
            normalized.append(trimmed);
        }
    }
    if (m_selectedTopics == normalized) {
        return;
    }
    m_selectedTopics = normalized;
    invalidateRows(wasActive);
    emit selectedTopicsChanged();
}

void MessageFilterModel::setDirection(const QString &direction)
{
    const bool wasActive = filterActive();
    const QString normalized = normalizedDirection(direction);
    if (m_direction == normalized) {
        return;
    }
    m_direction = normalized;
    invalidateRows(wasActive);
    emit directionChanged();
}

QVariantMap MessageFilterModel::rowAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return {};
    }

    const QModelIndex rowIndex = index(row, 0);
    QVariantMap result;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto role = roles.cbegin(); role != roles.cend(); ++role) {
        result.insert(QString::fromUtf8(role.value()), rowIndex.data(role.key()));
    }
    return result;
}

bool MessageFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!sourceModel()) {
        return false;
    }

    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString kind = sourceIndex.data(EventStreamModel::KindRole).toString();
    if (kind == QStringLiteral("divider")) {
        return !filterActive();
    }

    if (m_direction != QStringLiteral("all")
        && sourceIndex.data(EventStreamModel::DirectionRole).toString() != m_direction) {
        return false;
    }

    const QString topic = sourceIndex.data(EventStreamModel::TopicRole).toString();
    if (!m_selectedTopics.isEmpty()) {
        bool matched = false;
        for (const QString &filter : m_selectedTopics) {
            if (PayloadCodec::topicFilterMatches(filter, topic)) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            return false;
        }
    }

    if (m_filterText.isEmpty()) {
        return true;
    }

    const QString searchable = QStringLiteral("%1\n%2\n%3\n%4")
        .arg(
            sourceIndex.data(EventStreamModel::AliasRole).toString(),
            topic,
            sourceIndex.data(EventStreamModel::PayloadRole).toString(),
            sourceIndex.data(EventStreamModel::PayloadFormatRole).toString());
    return searchable.contains(m_filterText, Qt::CaseInsensitive);
}

void MessageFilterModel::invalidateRows(bool wasActive)
{
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    emit messageCountsChanged();
    if (wasActive != filterActive()) {
        emit filterActiveChanged();
    }
}

void MessageFilterModel::connectCountSignals()
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &MessageFilterModel::countChanged, Qt::UniqueConnection);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &MessageFilterModel::countChanged, Qt::UniqueConnection);
    connect(this, &QAbstractItemModel::modelReset, this, &MessageFilterModel::countChanged, Qt::UniqueConnection);
    connect(this, &QAbstractItemModel::layoutChanged, this, &MessageFilterModel::countChanged, Qt::UniqueConnection);
}

void MessageFilterModel::connectSourceSignals()
{
    for (const QMetaObject::Connection &connection : m_sourceConnections) {
        disconnect(connection);
    }
    m_sourceConnections.clear();

    QAbstractItemModel *source = sourceModel();
    if (!source) {
        return;
    }

    m_sourceConnections = {
        connect(source,
                &QAbstractItemModel::rowsInserted,
                this,
                &MessageFilterModel::messageCountsChanged),
        connect(source,
                &QAbstractItemModel::rowsRemoved,
                this,
                &MessageFilterModel::messageCountsChanged),
        connect(source,
                &QAbstractItemModel::modelReset,
                this,
                &MessageFilterModel::messageCountsChanged),
        connect(source,
                &QAbstractItemModel::layoutChanged,
                this,
                &MessageFilterModel::messageCountsChanged),
        connect(source,
                &QAbstractItemModel::dataChanged,
                this,
                &MessageFilterModel::messageCountsChanged),
    };
}

int MessageFilterModel::messageCount(const QAbstractItemModel *model)
{
    if (!model) {
        return 0;
    }
    int count = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 0).data(EventStreamModel::KindRole).toString() == QStringLiteral("message")) {
            ++count;
        }
    }
    return count;
}

QString MessageFilterModel::normalizedDirection(const QString &direction)
{
    return direction == QStringLiteral("incoming") || direction == QStringLiteral("outgoing")
        ? direction
        : QStringLiteral("all");
}
