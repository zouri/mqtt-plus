#pragma once

#include <QAbstractListModel>
#include <QString>
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

    void setScripts(const QVector<ScriptEntry> &scripts);

signals:
    void countChanged();

private:
    struct ScriptRow
    {
        QString id;
        QString name;
        QString description;
        QString code;
        QString updatedAt;
        QString filePath;
    };

    static ScriptRow rowFromScript(const ScriptEntry &script);

    QVector<ScriptRow> m_rows;
};
