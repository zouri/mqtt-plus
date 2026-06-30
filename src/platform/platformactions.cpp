#include "platform/platformactions.h"

#include <QAction>
#include <QClipboard>
#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QPoint>

namespace {
QPoint menuPosition(const QPointF &globalPosition)
{
    if (globalPosition.isNull()) {
        return QCursor::pos();
    }
    return globalPosition.toPoint();
}
} // namespace

QString PlatformActions::showSessionContextMenu(bool canDelete, const QPointF &globalPosition) const
{
    QMenu menu;
    QAction *editAction = menu.addAction(QCoreApplication::translate("PlatformActions", "Edit"));
    QAction *copyAction = menu.addAction(QCoreApplication::translate("PlatformActions", "Copy"));
    QAction *deleteAction = menu.addAction(QCoreApplication::translate("PlatformActions", "Delete"));
    editAction->setIcon(QIcon(QStringLiteral(":/qt/qml/MqttPlusApp/resources/edit.svg")));
    deleteAction->setIcon(QIcon(QStringLiteral(":/qt/qml/MqttPlusApp/resources/delete.svg")));
    deleteAction->setEnabled(canDelete);

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

QString PlatformActions::showSubscriptionContextMenu(const QPointF &globalPosition) const
{
    QMenu menu;
    QAction *editAction = menu.addAction(QCoreApplication::translate("PlatformActions", "Edit"));
    QAction *deleteAction = menu.addAction(QCoreApplication::translate("PlatformActions", "Delete"));

    QAction *selectedAction = menu.exec(menuPosition(globalPosition));
    if (selectedAction == editAction) {
        return QStringLiteral("edit");
    }
    if (selectedAction == deleteAction) {
        return QStringLiteral("delete");
    }
    return {};
}

void PlatformActions::copyTextToClipboard(const QString &text) const
{
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}
