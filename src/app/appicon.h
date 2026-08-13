#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

namespace AppIcon {

QColor themeColor(const QString &themeColorName);
QIcon themed(const QString &themeColorName);

} // namespace AppIcon
