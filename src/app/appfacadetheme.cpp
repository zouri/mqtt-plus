#include "app/appfacade.h"

void AppFacade::setThemeMode(const QString &mode)
{
    m_themeController.setMode(mode);
}

void AppFacade::setLanguageMode(const QString &mode)
{
    m_languageController.setMode(mode);
}
