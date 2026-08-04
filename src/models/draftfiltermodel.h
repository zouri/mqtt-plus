#pragma once

#include <QSortFilterProxyModel>
#include <QString>
#include <QVariantMap>

class DraftFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QString sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit DraftFilterModel(QObject *parent = nullptr);
    QString filterText() const;
    QString sortMode() const;
    int count() const;
    void setFilterText(const QString &filterText);
    void setSortMode(const QString &sortMode);
    Q_INVOKABLE QVariantMap rowAt(int row) const;

signals:
    void filterTextChanged();
    void sortModeChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QString m_filterText;
    QString m_normalizedFilterText;
    QString m_sortMode = QStringLiteral("name");
};
