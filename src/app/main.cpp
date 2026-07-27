#include <QDebug>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QVariant>

#include "app/application.h"
#include "app/windowgeometrymanager.h"

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QGuiApplication guiApplication(argc, argv);
    auto applicationFont = guiApplication.font();
    applicationFont.setFamily(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
    guiApplication.setFont(applicationFont);
    guiApplication.setWindowIcon(QIcon(QStringLiteral(":/assets/icons/app-icon.png")));

    Application application;

    QQmlApplicationEngine engine;
    QObject::connect(application.viewModel()->settings(), &SettingsViewModel::languageChanged, &engine, &QQmlApplicationEngine::retranslate);
    engine.setInitialProperties({
        {QStringLiteral("app"), QVariant::fromValue(application.viewModel())},
    });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &guiApplication,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("MqttPlusApp", "Main");

    const QList<QObject *> rootObjects = engine.rootObjects();
    if (rootObjects.isEmpty()) {
        qCritical() << "Failed to create the root QML object";
        return -1;
    }

    QQuickWindow *window = qobject_cast<QQuickWindow *>(rootObjects.constFirst());
    if (!window) {
        qCritical() << "The root QML object is not a QQuickWindow";
        return -1;
    }

    auto *windowGeometryManager = new WindowGeometryManager(
        *window,
        *application.viewModel()->preferences(),
        window);
    QObject::connect(
        &guiApplication,
        &QGuiApplication::aboutToQuit,
        windowGeometryManager,
        &WindowGeometryManager::saveNow);
    windowGeometryManager->restoreAndShow();

    return guiApplication.exec();
}
