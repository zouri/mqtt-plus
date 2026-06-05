#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QVariant>

#include "app/appfacade.h"

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/icons/app-icon.png")));

    AppFacade facade;

    QQmlApplicationEngine engine;
    QObject::connect(&facade, &AppFacade::languageChanged, &engine, &QQmlApplicationEngine::retranslate);
    engine.setInitialProperties({
        {QStringLiteral("app"), QVariant::fromValue(&facade)},
    });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("MqttPlusApp", "Main");

    return app.exec();
}
