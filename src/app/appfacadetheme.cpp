#include "app/appfacade.h"

void AppFacade::setThemeMode(const QString &mode)
{
    m_themeController.setMode(mode);
}
