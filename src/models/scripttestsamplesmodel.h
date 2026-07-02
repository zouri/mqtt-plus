#pragma once

#include <QAbstractListModel>
#include <QVariantList>

class ScriptTestSamplesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role : int {
        TopicRole = Qt::UserRole + 1,
        PayloadRole,
        FormatRole,
        FormatNameRole,
        TimestampRole,
        PayloadSizeRole,
    };
    Q_ENUM(Role)

    explicit ScriptTestSamplesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap rowAt(int row) const;

    void setSource(const QVariantList *messageRows);
    void notifyRefresh();

signals:
    void countChanged();

private:
    void rebuild(int newCount);

    const QVariantList *m_messageRows = nullptr;
    QVariantList m_empty;
    QVariantList m_sampleRows;
};
