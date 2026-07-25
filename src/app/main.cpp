#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QVariant>

#include "app/application.h"

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

    return guiApplication.exec();
}
