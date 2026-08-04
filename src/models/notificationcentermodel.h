#pragma once

#include <QAbstractListModel>
#include <QTimer>
#include <QVector>

class NotificationCenterModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role : int {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        MessageRole,
        SeverityRole,
        ActionLabelRole,
        ActionIdRole,
    };
    Q_ENUM(Role)

    explicit NotificationCenterModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void postOrUpdate(
        const QString &id,
        const QString &title,
        const QString &message,
        const QString &severity,
        int autoCloseMs = 0,
        const QString &actionLabel = QString(),
        const QString &actionId = QString());
    Q_INVOKABLE void dismiss(const QString &id);
    Q_INVOKABLE void setHovered(const QString &id, bool hovered);
    Q_INVOKABLE void triggerAction(const QString &id);

signals:
    void countChanged();
    void actionRequested(const QString &actionId);

private:
    struct Entry {
        QString id;
        QString title;
        QString message;
        QString severity;
        QString actionLabel;
        QString actionId;
        int remainingMs = 0;
        bool hovered = false;
    };

    static constexpr int kMaxVisible = 3;
    static constexpr int kMaxQueued = 32;
    int visibleIndex(const QString &id) const;
    int queuedIndex(const QString &id) const;
    void updateEntry(Entry &entry, const QString &title, const QString &message,
                     const QString &severity, int autoCloseMs,
                     const QString &actionLabel, const QString &actionId);
    void promoteQueued();
    void tick();

    QVector<Entry> m_visible;
    QVector<Entry> m_queued;
    QTimer m_tickTimer;
};
