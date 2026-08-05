#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVariantList>
#include <QVector>

class MessageProcessorEngine;
class ProcessorLibrary;

class ProcessorLibraryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role : int {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        LanguageIdRole,
        LanguageNameRole,
        RuntimeIdRole,
        CurrentRevisionIdRole,
        CurrentRevisionNumberRole,
        ReadinessStateRole,
        ReadinessDetailRole,
        ArchivedRole,
        ArchivedAtRole,
        UpdatedAtRole,
        SourceTextRole,
        RevisionsRole,
    };
    Q_ENUM(Role)

    explicit ProcessorLibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;
    Q_INVOKABLE int indexOfId(const QString &id) const;

    void refresh(ProcessorLibrary &library, MessageProcessorEngine &engine);

signals:
    void countChanged();

private:
    struct Row
    {
        QString id;
        QString name;
        QString description;
        QString languageId;
        QString languageName;
        QString runtimeId;
        QString currentRevisionId;
        qint64 currentRevisionNumber = 0;
        QString readinessState;
        QString readinessDetail;
        bool archived = false;
        QString archivedAt;
        QString updatedAt;
        QString sourceText;
        QVariantList revisions;
    };

    static QVariantMap rowToMap(const Row &row);

    QVector<Row> m_rows;
};
