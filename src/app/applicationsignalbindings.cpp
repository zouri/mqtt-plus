#include "app/applicationsignalbindings.h"

#include "app/applicationcontrollercontexts.h"
#include "services/apputils.h"
#include "app/applicationnotifier.h"
#include "app/applicationviewrefreshcoordinator.h"
#include "controllers/languagecontroller.h"
#include "controllers/preferencescontroller.h"
#include "controllers/scriptcontroller.h"
#include "controllers/subscriptioncontroller.h"
#include "controllers/themecontroller.h"

#include <QObject>
#include <QTimer>

using namespace AppUtils;

void ApplicationSignalBindings::setDependencies(const ApplicationSignalBindingDependencies &dependencies)
{
    m_dependencies = dependencies;
}

void ApplicationSignalBindings::install(QObject *owner)
{
    QObject::connect(
        m_dependencies.scriptController,
        &ScriptController::storageError,
        owner,
        [this](const QString &message) {
            m_dependencies.viewRefreshCoordinator->reportStorageError(message);
        });

    QObject::connect(m_dependencies.themeController, &ThemeController::modeChanged, m_dependencies.notifier, &ApplicationNotifier::notifyThemeModeChanged);
    QObject::connect(m_dependencies.themeController, &ThemeController::effectiveThemeChanged, m_dependencies.notifier, &ApplicationNotifier::notifyEffectiveThemeChanged);
    QObject::connect(m_dependencies.languageController, &LanguageController::modeChanged, m_dependencies.notifier, &ApplicationNotifier::notifyLanguageModeChanged);
    QObject::connect(m_dependencies.languageController, &LanguageController::languageChanged, owner, [this]() {
        m_dependencies.viewRefreshCoordinator->notifyLanguageChanged();
    });
    QObject::connect(m_dependencies.preferencesController, &PreferencesController::messageRetentionLimitChanged, m_dependencies.notifier, &ApplicationNotifier::notifyMessageRetentionLimitChanged);
    QObject::connect(m_dependencies.preferencesController, &PreferencesController::logRetentionLimitChanged, m_dependencies.notifier, &ApplicationNotifier::notifyLogRetentionLimitChanged);
    QObject::connect(m_dependencies.preferencesController, &PreferencesController::historyPageSizeChanged, owner, [this]() {
        m_dependencies.viewRefreshCoordinator->notifyHistoryPageSizeChanged();
    });
    QObject::connect(m_dependencies.preferencesController, &PreferencesController::maxIncomingPayloadBytesChanged, m_dependencies.notifier, &ApplicationNotifier::notifyMaxIncomingPayloadBytesChanged);
    QObject::connect(m_dependencies.preferencesController, &PreferencesController::deleteHistoryWithSessionChanged, m_dependencies.notifier, &ApplicationNotifier::notifyDeleteHistoryWithSessionChanged);
    QObject::connect(m_dependencies.preferencesController, &PreferencesController::saveMessagesWhenOutputPausedChanged, m_dependencies.notifier, &ApplicationNotifier::notifySaveMessagesWhenOutputPausedChanged);
    QObject::connect(m_dependencies.preferencesController, &PreferencesController::clearMessagesOnExitChanged, m_dependencies.notifier, &ApplicationNotifier::notifyClearMessagesOnExitChanged);
    QObject::connect(m_dependencies.preferencesController, &PreferencesController::clearLogsOnExitChanged, m_dependencies.notifier, &ApplicationNotifier::notifyClearLogsOnExitChanged);
    QObject::connect(m_dependencies.preferencesController, &PreferencesController::windowWidthChanged, m_dependencies.notifier, &ApplicationNotifier::notifyWindowWidthChanged);
    QObject::connect(m_dependencies.preferencesController, &PreferencesController::windowHeightChanged, m_dependencies.notifier, &ApplicationNotifier::notifyWindowHeightChanged);
    QObject::connect(m_dependencies.preferencesController, &PreferencesController::windowMaximizedChanged, m_dependencies.notifier, &ApplicationNotifier::notifyWindowMaximizedChanged);

    m_dependencies.subscriptionFpsRefreshTimer->setInterval(kSubscriptionFpsRefreshIntervalMs);
    QObject::connect(
        m_dependencies.subscriptionFpsRefreshTimer,
        &QTimer::timeout,
        m_dependencies.subscriptionController,
        &SubscriptionController::refreshSubscriptionFps);
}
