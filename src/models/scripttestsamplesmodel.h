#pragma once

#include <QAbstractListModel>
#include <QVector>

struct ScriptTestSampleRow {
    QString topic;
    QString payload;
    int format = 0;
    QString formatName;
    QString timestamp;
    int payloadSize = 0;
};

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

    void setRows(const QVector<ScriptTestSampleRow> &rows);

signals:
    void countChanged();

private:
    QVariantMap rowToMap(const ScriptTestSampleRow &row) const;

    QVector<ScriptTestSampleRow> m_rows;
};
