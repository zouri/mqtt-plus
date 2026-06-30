#include "app/applicationcore.h"

void ApplicationCore::setThemeMode(const QString &mode)
{
    m_themeController.setMode(mode);
}

void ApplicationCore::setLanguageMode(const QString &mode)
{
    m_languageController.setMode(mode);
}
