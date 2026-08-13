#include "app/appicon.h"

#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPixmap>

namespace AppIcon {

QColor themeColor(const QString &themeColorName)
{
    static const QHash<QString, QColor> themeColors {
        {QStringLiteral("mint"), QColor(QStringLiteral("#35d0aa"))},
        {QStringLiteral("blue"), QColor(QStringLiteral("#6aa3ff"))},
        {QStringLiteral("violet"), QColor(QStringLiteral("#ad8cff"))},
        {QStringLiteral("amber"), QColor(QStringLiteral("#f1b86a"))},
        {QStringLiteral("rose"), QColor(QStringLiteral("#ff879d"))},
    };
    return themeColors.value(themeColorName, themeColors.value(QStringLiteral("mint")));
}

QIcon themed(const QString &themeColorName)
{
    QImage icon(QStringLiteral(":/assets/icons/app-icon.png"));
    QImage themeMask(QStringLiteral(":/assets/icons/app-icon-theme-mask.png"));
    if (icon.isNull() || themeMask.isNull() || icon.size() != themeMask.size()) {
        return QIcon(QStringLiteral(":/assets/icons/app-icon.png"));
    }

    QPainter maskPainter(&themeMask);
    maskPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    maskPainter.fillRect(themeMask.rect(), themeColor(themeColorName));
    maskPainter.end();

    QPainter iconPainter(&icon);
    iconPainter.drawImage(0, 0, themeMask);
    iconPainter.end();
    return QIcon(QPixmap::fromImage(icon));
}

} // namespace AppIcon
