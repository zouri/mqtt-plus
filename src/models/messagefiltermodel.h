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

public:
    explicit MessageFilterModel(QObject *parent = nullptr);

    QString filterText() const;
    QStringList selectedTopics() const;
    QString direction() const;
    bool filterActive() const;
    int count() const;

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

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    void invalidateRows(bool wasActive);
    void connectCountSignals();
    static QString normalizedDirection(const QString &direction);

    QString m_filterText;
    QStringList m_selectedTopics;
    QString m_direction = QStringLiteral("all");
};
