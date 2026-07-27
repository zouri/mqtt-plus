#pragma once

#include <QMargins>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QTimer>
#include <QWindow>

class PreferencesController;
class QQuickWindow;

class WindowGeometryManager : public QObject
{
public:
    enum class SaveDisposition
    {
        Normal,
        Maximized,
        Preserve,
    };

    explicit WindowGeometryManager(
        QQuickWindow &window,
        PreferencesController &preferences,
        QObject *parent = nullptr);

    Q_DISABLE_COPY_MOVE(WindowGeometryManager)

    void restoreAndShow();
    void saveNow();

    static QRect centeredGeometry(
        const QSize &preferredSize,
        const QRect &availableGeometry,
        const QSize &minimumSize,
        const QMargins &frameMargins = {});
    static SaveDisposition saveDisposition(
        QWindow::Visibility visibility,
        Qt::WindowStates states);

private:
    void captureCurrentState();
    void scheduleSave();

    QQuickWindow &m_window;
    PreferencesController &m_preferences;
    QTimer m_saveTimer;
    QSize m_lastNormalSize;
    bool m_lastMaximized = false;
    bool m_restoring = false;
};
