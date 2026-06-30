#pragma once

#include <QPointF>
#include <QString>

class PlatformActions
{
public:
    QString showSessionContextMenu(bool canDelete, const QPointF &globalPosition) const;
    QString showSubscriptionContextMenu(const QPointF &globalPosition) const;
    void copyTextToClipboard(const QString &text) const;
};
