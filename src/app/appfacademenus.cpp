#include "app/appfacade.h"

#include <QAction>
#include <QActionGroup>
#include <QCursor>
#include <QList>
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

void AppFacade::showLanguageMenu(const QPointF &globalPosition)
{
    QMenu menu;
    QActionGroup languageGroup(&menu);
    languageGroup.setExclusive(true);

    QAction *systemAction = menu.addAction(tr("🌐 System"));
    QAction *englishAction = menu.addAction(tr("🇬🇧 English"));
    QAction *simplifiedChineseAction = menu.addAction(tr("🇨🇳 简体中文"));
    const QList<QAction *> languageActions = {
        systemAction,
        englishAction,
        simplifiedChineseAction,
    };

    systemAction->setData(QStringLiteral("system"));
    englishAction->setData(QStringLiteral("en"));
    simplifiedChineseAction->setData(QStringLiteral("zh_CN"));

    for (QAction *action : languageActions) {
        action->setCheckable(true);
        languageGroup.addAction(action);
        action->setChecked(action->data().toString() == m_languageController.mode());
    }

    QAction *selectedAction = menu.exec(menuPosition(globalPosition));
    if (!selectedAction) {
        return;
    }

    setLanguageMode(selectedAction->data().toString());
}
