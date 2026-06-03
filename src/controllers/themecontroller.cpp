#include "themecontroller.h"

#include "app/appfacadeutils.h"

#include <QGuiApplication>
#include <QStyleHints>

using namespace AppFacadeUtils;

ThemeController::ThemeController(QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    if (m_settings) {
        m_mode = sanitizeThemeMode(
            m_settings->value(QStringLiteral("appearance/themeMode"), QStringLiteral("system")).toString());
    }
    refreshSystemColorScheme();

    if (QGuiApplication::styleHints()) {
        connect(
            QGuiApplication::styleHints(),
            &QStyleHints::colorSchemeChanged,
            this,
            [this](Qt::ColorScheme) { refreshSystemColorScheme(); });
    }
}

QString ThemeController::mode() const
{
    return m_mode;
}

QString ThemeController::effectiveTheme() const
{
    if (m_mode == QStringLiteral("light") || m_mode == QStringLiteral("dark")) {
        return m_mode;
    }
    return m_systemDarkMode ? QStringLiteral("dark") : QStringLiteral("light");
}

void ThemeController::setMode(const QString &mode)
{
    const QString sanitized = sanitizeThemeMode(mode);
    if (sanitized == m_mode) {
        return;
    }

    const QString previousEffectiveTheme = effectiveTheme();
    m_mode = sanitized;
    if (m_settings) {
        m_settings->setValue(QStringLiteral("appearance/themeMode"), m_mode);
        m_settings->sync();
    }

    emit modeChanged();
    if (effectiveTheme() != previousEffectiveTheme) {
        emit effectiveThemeChanged();
    }
}

void ThemeController::refreshSystemColorScheme()
{
    const bool darkMode = QGuiApplication::styleHints()
        && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    if (darkMode == m_systemDarkMode) {
        return;
    }

    const QString previousEffectiveTheme = effectiveTheme();
    m_systemDarkMode = darkMode;
    if (effectiveTheme() != previousEffectiveTheme) {
        emit effectiveThemeChanged();
    }
}
