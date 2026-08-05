#pragma once

#include <QSortFilterProxyModel>
#include <QString>
#include <QVariantMap>

class ProcessorFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit ProcessorFilterModel(QObject *parent = nullptr);

    QString filterText() const;
    int count() const;
    void setFilterText(const QString &filterText);

    Q_INVOKABLE QVariantMap rowAt(int row) const;

signals:
    void filterTextChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_filterText;
    QString m_normalizedFilterText;
};
