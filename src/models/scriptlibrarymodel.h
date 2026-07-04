#pragma once

#include <QAbstractListModel>
#include <QVector>

struct ScriptEntry;

class ScriptLibraryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role : int {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        CodeRole,
        UpdatedAtRole,
        FilePathRole,
    };
    Q_ENUM(Role)

    explicit ScriptLibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;
    Q_INVOKABLE int indexOfId(const QString &id) const;

    void setSource(const QVector<ScriptEntry> *scripts);
    void notifyRefresh();

signals:
    void countChanged();

private:
    const QVector<ScriptEntry> *m_scripts = nullptr;
    int m_knownCount = 0;
};
