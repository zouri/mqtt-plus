#include "app/appfacade.h"

#include <QAction>
#include <QCursor>
#include <QIcon>
#include <QMenu>
#include <QPoint>
#include <QPointF>

namespace {
QPoint menuPosition(const QPointF &globalPosition)
{
    if (globalPosition.isNull()) {
        return QCursor::pos();
    }
    return globalPosition.toPoint();
}
} // namespace

QString AppFacade::showSessionContextMenu(int index, const QPointF &globalPosition)
{
    if (!m_sessionController.isValidIndex(index)) {
        return {};
    }

    QMenu menu;
    QAction *editAction = menu.addAction(tr("Edit"));
    QAction *copyAction = menu.addAction(tr("Copy"));
    QAction *deleteAction = menu.addAction(tr("Delete"));
    editAction->setIcon(QIcon(QStringLiteral(":/qt/qml/MqttPlusApp/resources/edit.svg")));
    deleteAction->setIcon(QIcon(QStringLiteral(":/qt/qml/MqttPlusApp/resources/delete.svg")));
    deleteAction->setEnabled(m_sessionController.sessions().size() > 1);

    QAction *selectedAction = menu.exec(menuPosition(globalPosition));
    if (selectedAction == editAction) {
        return QStringLiteral("edit");
    }
    if (selectedAction == copyAction) {
        return QStringLiteral("copy");
    }
    if (selectedAction == deleteAction) {
        return QStringLiteral("delete");
    }
    return {};
}

QString AppFacade::showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition)
{
    const SessionState *session = currentSessionState();
    if (!session || !subscriptionByTopic(session, topic.trimmed())) {
        return {};
    }

    QMenu menu;
    QAction *editAction = menu.addAction(tr("Edit"));
    QAction *deleteAction = menu.addAction(tr("Delete"));

    QAction *selectedAction = menu.exec(menuPosition(globalPosition));
    if (selectedAction == editAction) {
        return QStringLiteral("edit");
    }
    if (selectedAction == deleteAction) {
        return QStringLiteral("delete");
    }
    return {};
}
