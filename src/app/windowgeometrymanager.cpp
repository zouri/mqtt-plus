#include "windowgeometrymanager.h"

#include "usecases/preferencescontroller.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>

#include <algorithm>

WindowGeometryManager::WindowGeometryManager(
    QQuickWindow &window,
    PreferencesController &preferences,
    QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_preferences(preferences)
{
    m_saveTimer.setInterval(250);
    m_saveTimer.setSingleShot(true);
    QObject::connect(&m_saveTimer, &QTimer::timeout, this, &WindowGeometryManager::saveNow);

    QObject::connect(&m_window, &QWindow::widthChanged, this, &WindowGeometryManager::scheduleSave);
    QObject::connect(&m_window, &QWindow::heightChanged, this, &WindowGeometryManager::scheduleSave);
    QObject::connect(&m_window, &QWindow::windowStateChanged, this, &WindowGeometryManager::scheduleSave);
    QObject::connect(&m_window, &QWindow::visibilityChanged, this, &WindowGeometryManager::scheduleSave);
    QObject::connect(&m_window, &QQuickWindow::closing, this, &WindowGeometryManager::saveNow);
}

void WindowGeometryManager::restoreAndShow()
{
    m_restoring = true;
    m_window.create();

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        screen = m_window.screen();
    }
    if (screen && m_window.screen() != screen) {
        m_window.setScreen(screen);
    }
    const QRect availableGeometry = screen ? screen->availableGeometry() : QRect {};
    const QMargins frameMargins = m_window.frameMargins();
    const QSize savedSize = m_preferences.windowSize();
    const QSize preferredSize = savedSize.isValid() ? savedSize : m_window.size();

    const QRect restoredGeometry = availableGeometry.isValid()
        ? centeredGeometry(
              preferredSize,
              availableGeometry,
              m_window.minimumSize(),
              frameMargins)
        : QRect(QPoint {}, preferredSize);

    if (restoredGeometry.isValid()) {
        m_window.setGeometry(restoredGeometry);
        m_lastNormalSize = restoredGeometry.size();
    }

    m_lastMaximized = m_preferences.windowMaximized();
    m_preferences.setWindowState(m_lastNormalSize, m_lastMaximized);

    if (m_lastMaximized) {
        m_window.showMaximized();
    } else {
        m_window.show();
    }
    m_restoring = false;
}

void WindowGeometryManager::saveNow()
{
    if (m_restoring || !m_lastNormalSize.isValid()) {
        return;
    }

    m_saveTimer.stop();
    captureCurrentState();
    m_preferences.setWindowState(m_lastNormalSize, m_lastMaximized);
}

QRect WindowGeometryManager::centeredGeometry(
    const QSize &preferredSize,
    const QRect &availableGeometry,
    const QSize &minimumSize,
    const QMargins &frameMargins)
{
    if (!preferredSize.isValid() || !availableGeometry.isValid()) {
        return {};
    }

    const int leftMargin = (std::max)(0, frameMargins.left());
    const int topMargin = (std::max)(0, frameMargins.top());
    const int rightMargin = (std::max)(0, frameMargins.right());
    const int bottomMargin = (std::max)(0, frameMargins.bottom());
    const int maximumWidth = (std::max)(1, availableGeometry.width() - leftMargin - rightMargin);
    const int maximumHeight = (std::max)(1, availableGeometry.height() - topMargin - bottomMargin);
    const int minimumWidth = (std::clamp)(minimumSize.width(), 1, maximumWidth);
    const int minimumHeight = (std::clamp)(minimumSize.height(), 1, maximumHeight);
    const int width = (std::clamp)(preferredSize.width(), minimumWidth, maximumWidth);
    const int height = (std::clamp)(preferredSize.height(), minimumHeight, maximumHeight);
    const int frameWidth = width + leftMargin + rightMargin;
    const int frameHeight = height + topMargin + bottomMargin;
    return QRect(
        availableGeometry.left()
            + (availableGeometry.width() - frameWidth) / 2
            + leftMargin,
        availableGeometry.top()
            + (availableGeometry.height() - frameHeight) / 2
            + topMargin,
        width,
        height);
}

WindowGeometryManager::SaveDisposition WindowGeometryManager::saveDisposition(
    QWindow::Visibility visibility,
    Qt::WindowStates states)
{
    if (states.testFlag(Qt::WindowFullScreen)
        || states.testFlag(Qt::WindowMinimized)
        || visibility == QWindow::FullScreen
        || visibility == QWindow::Minimized
        || visibility == QWindow::Hidden) {
        return SaveDisposition::Preserve;
    }
    if (states.testFlag(Qt::WindowMaximized) || visibility == QWindow::Maximized) {
        return SaveDisposition::Maximized;
    }
    return visibility == QWindow::Windowed
        ? SaveDisposition::Normal
        : SaveDisposition::Preserve;
}

void WindowGeometryManager::captureCurrentState()
{
    const SaveDisposition disposition = saveDisposition(
        m_window.visibility(),
        m_window.windowStates());
    if (disposition == SaveDisposition::Normal && m_window.size().isValid()) {
        m_lastNormalSize = m_window.size();
        m_lastMaximized = false;
    } else if (disposition == SaveDisposition::Maximized) {
        m_lastMaximized = true;
    }
}

void WindowGeometryManager::scheduleSave()
{
    if (m_restoring) {
        return;
    }

    m_saveTimer.start();
}
