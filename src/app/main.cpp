#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QVariant>

#include "app/applicationobjectgraph.h"

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QGuiApplication app(argc, argv);
    auto applicationFont = app.font();
    applicationFont.setFamily(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
    app.setFont(applicationFont);
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/icons/app-icon.png")));

    ApplicationObjectGraph objectGraph;

    QQmlApplicationEngine engine;
    QObject::connect(objectGraph.settingsViewModel(), &SettingsViewModel::languageChanged, &engine, &QQmlApplicationEngine::retranslate);
    engine.setInitialProperties({
        {QStringLiteral("app"), QVariant::fromValue(objectGraph.viewModel())},
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
