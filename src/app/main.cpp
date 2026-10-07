#include <QDebug>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QVariant>

#include "app/application.h"
#include "app/appicon.h"
#include "app/windowgeometrymanager.h"

#if defined(QT_QML_DEBUG) || defined(MQTT_PLUS_ENABLE_PROFILING)
#include <QStandardPaths>
#include <QTemporaryDir>
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

#if defined(QT_QML_DEBUG) || defined(MQTT_PLUS_ENABLE_PROFILING)
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
    const auto localBrokerPort = positiveOptionValue(
        arguments, QStringLiteral("--profile-local-broker-port"), 0);
    const auto observationMs = positiveOptionValue(
        arguments, QStringLiteral("--profile-observation-ms"), 0);
    if (!messageCount || !batchSize || !intervalMs || !payloadBytes
        || !localBrokerPort || *localBrokerPort > 65535
        || !observationMs || *observationMs > 600000) {
        qCritical().noquote()
            << "Invalid message stream profiler numeric options";
        request.valid = false;
        return request;
    }

    request.options.messageCount = *messageCount;
    request.options.batchSize = *batchSize;
    request.options.intervalMs = *intervalMs;
    request.options.payloadBytes = *payloadBytes;
    request.options.pauseOutput = arguments.contains(QStringLiteral("--profile-pause-output"));
    request.options.varyPayload = arguments.contains(QStringLiteral("--profile-vary-payload"));
    request.options.localBrokerPort = *localBrokerPort;
    request.options.observationMs = *observationMs;
    request.options.brokerInput = arguments.contains(QStringLiteral("--profile-broker-input"));
    request.options.scrollOutput = arguments.contains(QStringLiteral("--profile-scroll-output"));
    request.options.activateWindow = arguments.contains(QStringLiteral("--profile-activate-window"));
    request.options.backgroundWindow = arguments.contains(QStringLiteral("--profile-background-window"));
    request.options.captureWindow = arguments.contains(QStringLiteral("--profile-capture-window"));
    if (request.options.activateWindow && request.options.backgroundWindow) {
        qCritical().noquote() << "Foreground and background profiling options are mutually exclusive";
        request.valid = false;
    }
    if (request.options.scrollOutput && request.options.pauseOutput) {
        qCritical().noquote() << "Scrolling profiling requires active output";
        request.valid = false;
    }
    if (request.options.brokerInput && request.options.localBrokerPort == 0) {
        qCritical().noquote() << "Broker input profiling requires an explicit local broker port";
        request.valid = false;
    }
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

#if !defined(QT_QML_DEBUG) && !defined(MQTT_PLUS_ENABLE_PROFILING)
    if (guiApplication.arguments().contains(QStringLiteral("--profile-message-stream"))) {
        qCritical().noquote() << "Synthetic profiling requires a profiling-enabled build";
        return 2;
    }
#endif

    QSettings::Format applicationSettingsFormat = QSettings::NativeFormat;
#if defined(QT_QML_DEBUG) || defined(MQTT_PLUS_ENABLE_PROFILING)
    const auto profileRequest = messageStreamProfileRequest(guiApplication.arguments());
    if (!profileRequest.valid) {
        return 2;
    }
    std::unique_ptr<QTemporaryDir> profileSettingsDirectory;
    if (profileRequest.requested) {
        QStandardPaths::setTestModeEnabled(true);
        profileSettingsDirectory = std::make_unique<QTemporaryDir>();
        if (!profileSettingsDirectory->isValid()) {
            qCritical().noquote() << "Cannot create isolated profiler settings";
            return 2;
        }
        applicationSettingsFormat = QSettings::IniFormat;
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
            profileSettingsDirectory->path());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope,
            profileSettingsDirectory->path());
        QSettings profileSettings(QSettings::IniFormat, QSettings::UserScope,
            QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"));
        profileSettings.setFallbacksEnabled(false);
        profileSettings.setValue(QStringLiteral("updates/automaticChecksEnabled"), false);
        profileSettings.setValue(QStringLiteral("workspace/contextPane"), QStringLiteral("subscriptions"));
        profileSettings.sync();
        if (profileSettings.status() != QSettings::NoError) {
            qCritical().noquote() << "Cannot initialize isolated profiler settings";
            return 2;
        }
        qInfo().noquote() << "PROFILE_MESSAGE_STREAM settings isolated format=ini updates=disabled";
    }
#endif

    Application application(applicationSettingsFormat);
    SettingsViewModel *settingsViewModel = application.viewModel()->settings();
    const auto applyConfiguredIcon = [&guiApplication, settingsViewModel]() {
        const QIcon icon = AppIcon::themed(settingsViewModel->themeColor());
        guiApplication.setWindowIcon(icon);
        for (QWindow *window : guiApplication.allWindows()) {
            window->setIcon(icon);
        }
    };
    applyConfiguredIcon();
    QObject::connect(
        settingsViewModel,
        &SettingsViewModel::themeColorChanged,
        &guiApplication,
        applyConfiguredIcon);
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
    applyConfiguredIcon();

#if defined(QT_QML_DEBUG) || defined(MQTT_PLUS_ENABLE_PROFILING)
    if (profileRequest.requested && profileRequest.options.backgroundWindow) {
        window->setFlag(Qt::WindowDoesNotAcceptFocus);
    }
#endif

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

#if defined(QT_QML_DEBUG) || defined(MQTT_PLUS_ENABLE_PROFILING)
    std::unique_ptr<MessageStreamProfileDriver> profileDriver;
    if (profileRequest.requested) {
        profileDriver = std::make_unique<MessageStreamProfileDriver>(
            *application.viewModel(),
            profileRequest.options,
            *window,
            &guiApplication);
        QObject::connect(
            window,
            &QQuickWindow::frameSwapped,
            profileDriver.get(),
            &MessageStreamProfileDriver::recordRenderedFrame,
            Qt::QueuedConnection);
        QTimer::singleShot(
            1000,
            profileDriver.get(),
            &MessageStreamProfileDriver::start);
    }
#endif

    return guiApplication.exec();
}
