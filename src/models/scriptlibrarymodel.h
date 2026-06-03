#pragma once

#include <QAbstractListModel>
#include <QVector>

struct ScriptLibraryRow {
    QString id;
    QString name;
    QString code;
    QString updatedAt;
    QString filePath;
};

class ScriptLibraryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        CodeRole,
        UpdatedAtRole,
        FilePathRole
    };
    Q_ENUM(Role)

    explicit ScriptLibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;
    Q_INVOKABLE int indexOfId(const QString &id) const;

    void setRows(const QVector<ScriptLibraryRow> &rows);

signals:
    void countChanged();

private:
    QVariantMap rowToMap(const ScriptLibraryRow &row) const;

    QVector<ScriptLibraryRow> m_rows;
};
