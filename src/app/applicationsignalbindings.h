#pragma once

class ApplicationControllerContexts;
class ApplicationNotifier;
class ApplicationViewRefreshCoordinator;
class LanguageController;
class PreferencesController;
class QObject;
class QTimer;
class ScriptController;
class SubscriptionController;
class ThemeController;

struct ApplicationSignalBindingDependencies
{
    ApplicationControllerContexts *controllerContexts = nullptr;
    ApplicationNotifier *notifier = nullptr;
    ApplicationViewRefreshCoordinator *viewRefreshCoordinator = nullptr;
    LanguageController *languageController = nullptr;
    PreferencesController *preferencesController = nullptr;
    ScriptController *scriptController = nullptr;
    SubscriptionController *subscriptionController = nullptr;
    ThemeController *themeController = nullptr;
    QTimer *subscriptionFpsRefreshTimer = nullptr;
};

class ApplicationSignalBindings
{
public:
    void setDependencies(const ApplicationSignalBindingDependencies &dependencies);
    void install(QObject *owner);

private:
    ApplicationSignalBindingDependencies m_dependencies;
};
