#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QVariant>

#include "app/applicationkernel.h"

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/icons/app-icon.png")));

    ApplicationKernel kernel;

    QQmlApplicationEngine engine;
    QObject::connect(kernel.events(), &UiEventHub::languageChanged, &engine, &QQmlApplicationEngine::retranslate);
    engine.setInitialProperties({
        {QStringLiteral("app"), QVariant::fromValue(kernel.appBus())},
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
