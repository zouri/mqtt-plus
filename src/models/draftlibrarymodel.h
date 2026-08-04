#pragma once

#include "domain/publishdraft.h"

#include <QAbstractListModel>
#include <QVector>

class DraftLibraryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role : int {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        DefaultTopicRole,
        PayloadRole,
        PayloadPreviewRole,
        FormatIdRole,
        FormatRole,
        FormatNameRole,
        QosRole,
        RetainRole,
        CreatedAtRole,
        UpdatedAtRole,
        LastUsedAtRole,
    };
    Q_ENUM(Role)

    explicit DraftLibraryModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantMap rowAt(int row) const;
    Q_INVOKABLE int indexOfId(const QString &id) const;
    void setDrafts(const QVector<PublishDraft> &drafts);

signals:
    void countChanged();

private:
    QVector<PublishDraft> m_drafts;
};
