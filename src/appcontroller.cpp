#include "appcontroller.h"

#include "appcontrollerutils.h"

#include <QGuiApplication>
#include <QStyleHints>

using namespace AppControllerUtils;

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"))
{
    m_launchTimestamp = timestampNow();
    m_themeMode = sanitizeThemeMode(
        m_settings.value(QStringLiteral("appearance/themeMode"), QStringLiteral("system")).toString());
    refreshSystemColorScheme();

    if (QGuiApplication::styleHints()) {
        connect(
            QGuiApplication::styleHints(),
            &QStyleHints::colorSchemeChanged,
            this,
            [this](Qt::ColorScheme) { refreshSystemColorScheme(); });
    }

    m_subscriptionFpsRefreshTimer.setInterval(kSubscriptionFpsRefreshIntervalMs);
    connect(
        &m_subscriptionFpsRefreshTimer,
        &QTimer::timeout,
        this,
        &AppController::refreshSubscriptionFps);
    loadScripts();
    loadSessions();
}
