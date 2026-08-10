#include <QDebug>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QVariant>

#include "app/application.h"
#include "app/windowgeometrymanager.h"

#ifdef QT_QML_DEBUG
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>

#include "app/messagestreamprofiledriver.h"

#include <memory>
#include <optional>
#endif

namespace {
QFont applicationFont(const QFont &baseFont, const QString &family)
{
    auto font = baseFont;
    font.setFamily(family);
    return font;
}

#ifdef QT_QML_DEBUG
struct MessageStreamProfileRequest
{
    bool requested = false;
    bool valid = true;
    MessageStreamProfileOptions options;
};

std::optional<int> positiveOptionValue(
    const QStringList &arguments,
    const QString &option,
    int defaultValue)
{
    const QString valuePrefix = option + QLatin1Char('=');
    for (int index = 0; index < arguments.size(); ++index) {
        const QString &argument = arguments.at(index);
        QString value;
        if (argument.startsWith(valuePrefix)) {
            value = argument.sliced(valuePrefix.size());
        } else if (argument == option) {
            if (index + 1 >= arguments.size()) {
                return std::nullopt;
            }
            value = arguments.at(index + 1);
        } else {
            continue;
        }

        bool ok = false;
        const int parsed = value.toInt(&ok);
        return ok && parsed > 0 ? std::optional<int>(parsed) : std::nullopt;
    }
    return defaultValue;
}

MessageStreamProfileRequest messageStreamProfileRequest(
    const QStringList &arguments)
{
    MessageStreamProfileRequest request;
    if (!arguments.contains(QStringLiteral("--profile-message-stream"))) {
        return request;
    }
    request.requested = true;

    const auto messageCount = positiveOptionValue(
        arguments,
        QStringLiteral("--profile-message-count"),
        request.options.messageCount);
    const auto batchSize = positiveOptionValue(
        arguments,
        QStringLiteral("--profile-message-batch"),
        request.options.batchSize);
    const auto intervalMs = positiveOptionValue(
        arguments,
        QStringLiteral("--profile-message-interval"),
        request.options.intervalMs);
    const auto payloadBytes = positiveOptionValue(
        arguments,
        QStringLiteral("--profile-payload-bytes"),
        request.options.payloadBytes);
    if (!messageCount || !batchSize || !intervalMs || !payloadBytes) {
        qCritical().noquote()
            << "Invalid message stream profiler options; all numeric values must be positive";
        request.valid = false;
        return request;
    }

    request.options.messageCount = *messageCount;
    request.options.batchSize = *batchSize;
    request.options.intervalMs = *intervalMs;
    request.options.payloadBytes = *payloadBytes;
    return request;
}
#endif
} // namespace

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QGuiApplication guiApplication(argc, argv);
    QCoreApplication::setApplicationVersion(QStringLiteral(MQTT_PLUS_VERSION));
    const QFont baseApplicationFont = guiApplication.font();
    guiApplication.setWindowIcon(QIcon(QStringLiteral(":/assets/icons/app-icon.png")));

#ifdef QT_QML_DEBUG
    const auto profileRequest = messageStreamProfileRequest(guiApplication.arguments());
    if (!profileRequest.valid) {
        return 2;
    }
    if (profileRequest.requested) {
        QStandardPaths::setTestModeEnabled(true);
    }
#endif

    Application application;
    SettingsViewModel *settingsViewModel = application.viewModel()->settings();
    const auto applyConfiguredFont = [&guiApplication, &baseApplicationFont, settingsViewModel]() {
        guiApplication.setFont(
            applicationFont(baseApplicationFont, settingsViewModel->effectiveFontFamily()));
    };
    applyConfiguredFont();
    QObject::connect(
        settingsViewModel,
        &SettingsViewModel::fontFamilyChanged,
        &guiApplication,
        applyConfiguredFont);

    QQmlApplicationEngine engine;
    QObject::connect(settingsViewModel, &SettingsViewModel::languageChanged, &engine, &QQmlApplicationEngine::retranslate);
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

#ifdef QT_QML_DEBUG
    std::unique_ptr<MessageStreamProfileDriver> profileDriver;
    if (profileRequest.requested) {
        profileDriver = std::make_unique<MessageStreamProfileDriver>(
            *application.viewModel(),
            profileRequest.options,
            &guiApplication);
        QTimer::singleShot(
            1000,
            profileDriver.get(),
            &MessageStreamProfileDriver::start);
    }
#endif

    return guiApplication.exec();
}
