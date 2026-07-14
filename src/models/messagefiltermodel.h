#pragma once

#include <QSortFilterProxyModel>
#include <QStringList>
#include <QVariantMap>

class MessageFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QStringList selectedTopics READ selectedTopics WRITE setSelectedTopics NOTIFY selectedTopicsChanged)
    Q_PROPERTY(QString direction READ direction WRITE setDirection NOTIFY directionChanged)
    Q_PROPERTY(bool filterActive READ filterActive NOTIFY filterActiveChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int filteredMessageCount READ filteredMessageCount NOTIFY messageCountsChanged)
    Q_PROPERTY(int totalMessageCount READ totalMessageCount NOTIFY messageCountsChanged)

public:
    explicit MessageFilterModel(QObject *parent = nullptr);

    QString filterText() const;
    QStringList selectedTopics() const;
    QString direction() const;
    bool filterActive() const;
    int count() const;
    int filteredMessageCount() const;
    int totalMessageCount() const;

    void setSourceModel(QAbstractItemModel *sourceModel) override;
    void setFilterText(const QString &filterText);
    void setSelectedTopics(const QStringList &selectedTopics);
    void setDirection(const QString &direction);

    Q_INVOKABLE QVariantMap rowAt(int row) const;

signals:
    void filterTextChanged();
    void selectedTopicsChanged();
    void directionChanged();
    void filterActiveChanged();
    void countChanged();
    void messageCountsChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    void invalidateRows(bool wasActive);
    void connectCountSignals();
    void connectSourceSignals();
    static int messageCount(const QAbstractItemModel *model);
    static QString normalizedDirection(const QString &direction);

    QString m_filterText;
    QStringList m_selectedTopics;
    QString m_direction = QStringLiteral("all");
};
