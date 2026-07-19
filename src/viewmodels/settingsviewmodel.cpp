#include "viewmodels/settingsviewmodel.h"

#include "usecases/eventhistoryservice.h"
#include "usecases/preferencescontroller.h"
#include "models/eventstreammodel.h"
#include "services/apputils.h"
#include "services/storage/historystore.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QStyleHints>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <utility>

using namespace AppUtils;

namespace {

enum class SettingsOption
{
    ThemeMode,
    LanguageMode,
    MessagePayloadDisplayMode,
    MessageRetentionLimit,
    LogRetentionLimit,
    HistoryPageSize,
    MaxIncomingPayloadBytes,
    CleanupMode,
};

const QVariantList &optionValues(SettingsOption option)
{
    static const QVariantList themeModes {
        QStringLiteral("system"),
        QStringLiteral("light"),
        QStringLiteral("dark"),
    };
    static const QVariantList languageModes {
        QStringLiteral("system"),
        QStringLiteral("en"),
        QStringLiteral("zh_CN"),
    };
    static const QVariantList messagePayloadDisplayModes {
        QStringLiteral("compact"),
        QStringLiteral("hover"),
        QStringLiteral("full"),
    };
    static const QVariantList messageRetentionLimits {1000, 5000, 10000, 0};
    static const QVariantList logRetentionLimits {500, 2000, 5000, 0};
    static const QVariantList historyPageSizes {200, 500, 1000};
    static const QVariantList maxIncomingPayloadBytes {262144, 1048576, 5242880, 16777216};
    static const QVariantList cleanupModes {
        QStringLiteral("never"),
        QStringLiteral("current"),
        QStringLiteral("all"),
    };

    switch (option) {
    case SettingsOption::ThemeMode:
        return themeModes;
    case SettingsOption::LanguageMode:
        return languageModes;
    case SettingsOption::MessagePayloadDisplayMode:
        return messagePayloadDisplayModes;
    case SettingsOption::MessageRetentionLimit:
        return messageRetentionLimits;
    case SettingsOption::LogRetentionLimit:
        return logRetentionLimits;
    case SettingsOption::HistoryPageSize:
        return historyPageSizes;
    case SettingsOption::MaxIncomingPayloadBytes:
        return maxIncomingPayloadBytes;
    case SettingsOption::CleanupMode:
        return cleanupModes;
    }
    return themeModes;
}

int optionIndex(SettingsOption option, const QVariant &value)
{
    const QVariantList &values = optionValues(option);
    for (int i = 0; i < values.size(); ++i) {
        if (values.at(i) == value) {
            return i;
        }
    }
    return 0;
}

QVariant optionValue(SettingsOption option, int index)
{
    const QVariantList &values = optionValues(option);
    if (values.isEmpty()) {
        return {};
    }
    return values.at(std::clamp(index, 0, static_cast<int>(values.size()) - 1));
}

void bindDependencyChange(
    const std::function<void(QObject *, std::function<void()>)> &binder,
    QObject *context,
    std::function<void()> handler)
{
    if (binder) {
        binder(context, std::move(handler));
    }
}

void clearSessionMessages(QVector<SessionState> *sessions)
{
    if (!sessions) {
        return;
    }

    for (auto &session : *sessions) {
        session.runtime.messageRows.clear();
        session.runtime.totalMessageCount = 0;
        session.runtime.viewedMessageCount = 0;
        session.runtime.oldestLoadedMessageId = 0;
        session.runtime.loadedAllMessageHistory = true;
    }
}

void clearSessionLogs(QVector<SessionState> *sessions)
{
    if (!sessions) {
        return;
    }

    for (auto &session : *sessions) {
        session.runtime.logRows.clear();
        session.runtime.oldestLoadedLogId = 0;
        session.runtime.loadedAllLogHistory = true;
    }
}

void notifyMessagesCleared(SettingsViewModel::Dependencies &dependencies)
{
    if (dependencies.messages) {
        dependencies.messages->clear();
    }
    if (dependencies.refreshScriptTestSamplesModel) {
        dependencies.refreshScriptTestSamplesModel();
    }
    if (dependencies.emitMessageStreamChanged) {
        dependencies.emitMessageStreamChanged();
    }
}

void notifyLogsCleared(SettingsViewModel::Dependencies &dependencies)
{
    if (dependencies.logs) {
        dependencies.logs->clear();
    }
    if (dependencies.emitLogStreamChanged) {
        dependencies.emitLogStreamChanged();
    }
}

QString sanitizeLanguageMode(const QString &value)
{
    const QString mode = value.trimmed();
    if (mode == QStringLiteral("en") || mode == QStringLiteral("zh_CN")) {
        return mode;
    }
    return QStringLiteral("system");
}

QString sanitizeThemeColor(const QString &value)
{
    static const QStringList themeColors {
        QStringLiteral("mint"),
        QStringLiteral("blue"),
        QStringLiteral("violet"),
        QStringLiteral("amber"),
        QStringLiteral("rose"),
    };
    const QString color = value.trimmed().toLower();
    return themeColors.contains(color) ? color : QStringLiteral("mint");
}

QString sanitizeMessagePayloadDisplayMode(const QString &value)
{
    const QString mode = value.trimmed().toLower();
    if (mode == QStringLiteral("compact") || mode == QStringLiteral("full")) {
        return mode;
    }
    return QStringLiteral("hover");
}

} // namespace

SettingsViewModel::SettingsViewModel(QSettings *settings, QObject *parent)
    : SettingsViewModel(Dependencies{}, settings, parent)
{
}

SettingsViewModel::SettingsViewModel(const Dependencies &dependencies, QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_dependencies(dependencies)
{
    if (m_settings) {
        m_themeMode = sanitizeThemeMode(
            m_settings->value(QStringLiteral("appearance/themeMode"), QStringLiteral("system")).toString());
        m_themeColor = sanitizeThemeColor(
            m_settings->value(QStringLiteral("appearance/themeColor"), QStringLiteral("mint")).toString());
        m_languageMode = sanitizeLanguageMode(
            m_settings->value(QStringLiteral("appearance/languageMode"), QStringLiteral("system")).toString());
        m_messagePayloadDisplayMode = sanitizeMessagePayloadDisplayMode(
            m_settings->value(QStringLiteral("workbench/messagePayloadDisplayMode"), QStringLiteral("hover")).toString());
    }
    refreshSystemColorScheme();
    applyCurrentLanguage();

    if (QGuiApplication::styleHints()) {
        connect(
            QGuiApplication::styleHints(),
            &QStyleHints::colorSchemeChanged,
            this,
            [this](Qt::ColorScheme) { refreshSystemColorScheme(); });
    }

    bindDependencyChange(m_dependencies.bindMessageRetentionLimitChanged, this, [this]() { emit messageRetentionLimitChanged(); });
    bindDependencyChange(m_dependencies.bindLogRetentionLimitChanged, this, [this]() { emit logRetentionLimitChanged(); });
    bindDependencyChange(m_dependencies.bindHistoryPageSizeChanged, this, [this]() { emit historyPageSizeChanged(); });
    bindDependencyChange(m_dependencies.bindMaxIncomingPayloadBytesChanged, this, [this]() { emit maxIncomingPayloadBytesChanged(); });
    bindDependencyChange(m_dependencies.bindDeleteHistoryWithSessionChanged, this, [this]() { emit deleteHistoryWithSessionChanged(); });
    bindDependencyChange(m_dependencies.bindSaveMessagesWhenOutputPausedChanged, this, [this]() { emit saveMessagesWhenOutputPausedChanged(); });
    bindDependencyChange(m_dependencies.bindAutoCollapseConnectionListOnConnectChanged, this, [this]() { emit autoCollapseConnectionListOnConnectChanged(); });
    bindDependencyChange(m_dependencies.bindClearMessagesOnExitChanged, this, [this]() { emit clearMessagesOnExitChanged(); });
    bindDependencyChange(m_dependencies.bindClearLogsOnExitChanged, this, [this]() { emit clearLogsOnExitChanged(); });
    bindDependencyChange(m_dependencies.bindWindowWidthChanged, this, [this]() { emit windowWidthChanged(); });
    bindDependencyChange(m_dependencies.bindWindowHeightChanged, this, [this]() { emit windowHeightChanged(); });
    bindDependencyChange(m_dependencies.bindWindowMaximizedChanged, this, [this]() { emit windowMaximizedChanged(); });
}

QString SettingsViewModel::themeMode() const { return m_themeMode; }

QString SettingsViewModel::themeColor() const { return m_themeColor; }

QString SettingsViewModel::effectiveTheme() const
{
    if (m_themeMode == QStringLiteral("light") || m_themeMode == QStringLiteral("dark")) {
        return m_themeMode;
    }
    return m_systemDarkMode ? QStringLiteral("dark") : QStringLiteral("light");
}

QString SettingsViewModel::languageMode() const { return m_languageMode; }

QVariantList SettingsViewModel::availableLanguages() const
{
    QVariantList languages;
    {
        QVariantMap system;
        system.insert(QStringLiteral("mode"), QStringLiteral("system"));
        system.insert(QStringLiteral("label"), tr("System"));
        languages.append(system);
    }
    {
        QVariantMap english;
        english.insert(QStringLiteral("mode"), QStringLiteral("en"));
        english.insert(QStringLiteral("label"), QStringLiteral("English"));
        languages.append(english);
    }
    {
        QVariantMap simplifiedChinese;
        simplifiedChinese.insert(QStringLiteral("mode"), QStringLiteral("zh_CN"));
        simplifiedChinese.insert(QStringLiteral("label"), QStringLiteral("简体中文"));
        languages.append(simplifiedChinese);
    }
    return languages;
}

QString SettingsViewModel::effectiveLanguage() const { return m_effectiveLanguage; }

int SettingsViewModel::messageRetentionLimit() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->messageRetentionLimit() : 5000; }
int SettingsViewModel::logRetentionLimit() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->logRetentionLimit() : 2000; }
int SettingsViewModel::historyPageSize() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->historyPageSize() : 500; }
int SettingsViewModel::maxIncomingPayloadBytes() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->maxIncomingPayloadBytes() : 1024 * 1024; }
bool SettingsViewModel::deleteHistoryWithSession() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->deleteHistoryWithSession() : true; }
bool SettingsViewModel::saveMessagesWhenOutputPaused() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->saveMessagesWhenOutputPaused() : true; }
bool SettingsViewModel::autoCollapseConnectionListOnConnect() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->autoCollapseConnectionListOnConnect() : true; }
QString SettingsViewModel::clearMessagesOnExit() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->clearMessagesOnExit() : QStringLiteral("never"); }
QString SettingsViewModel::clearLogsOnExit() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->clearLogsOnExit() : QStringLiteral("never"); }
int SettingsViewModel::windowWidth() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->windowWidth() : 1480; }
int SettingsViewModel::windowHeight() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->windowHeight() : 820; }
bool SettingsViewModel::windowMaximized() const { return m_dependencies.preferencesController && m_dependencies.preferencesController->windowMaximized(); }
int SettingsViewModel::subscriptionPaneWidth() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->subscriptionPaneWidth() : 320; }
int SettingsViewModel::publishComposerHeight() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->publishComposerHeight() : 168; }
bool SettingsViewModel::connectionPaneCollapsed() const { return m_dependencies.preferencesController && m_dependencies.preferencesController->connectionPaneCollapsed(); }
bool SettingsViewModel::subscriptionPaneCollapsed() const { return m_dependencies.preferencesController && m_dependencies.preferencesController->subscriptionPaneCollapsed(); }
int SettingsViewModel::themeModeIndex() const { return optionIndex(SettingsOption::ThemeMode, themeMode()); }
int SettingsViewModel::languageModeIndex() const { return optionIndex(SettingsOption::LanguageMode, languageMode()); }
int SettingsViewModel::messagePayloadDisplayModeIndex() const { return optionIndex(SettingsOption::MessagePayloadDisplayMode, m_messagePayloadDisplayMode); }
int SettingsViewModel::messageRetentionLimitIndex() const { return optionIndex(SettingsOption::MessageRetentionLimit, messageRetentionLimit()); }
int SettingsViewModel::logRetentionLimitIndex() const { return optionIndex(SettingsOption::LogRetentionLimit, logRetentionLimit()); }
int SettingsViewModel::historyPageSizeIndex() const { return optionIndex(SettingsOption::HistoryPageSize, historyPageSize()); }
int SettingsViewModel::maxIncomingPayloadBytesIndex() const { return optionIndex(SettingsOption::MaxIncomingPayloadBytes, maxIncomingPayloadBytes()); }
int SettingsViewModel::clearMessagesOnExitIndex() const { return optionIndex(SettingsOption::CleanupMode, clearMessagesOnExit()); }
int SettingsViewModel::clearLogsOnExitIndex() const { return optionIndex(SettingsOption::CleanupMode, clearLogsOnExit()); }

void SettingsViewModel::setThemeMode(const QString &mode)
{
    const QString sanitized = sanitizeThemeMode(mode);
    if (sanitized == m_themeMode) {
        return;
    }

    const QString previousEffectiveTheme = effectiveTheme();
    m_themeMode = sanitized;
    if (m_settings) {
        m_settings->setValue(QStringLiteral("appearance/themeMode"), m_themeMode);
        m_settings->sync();
    }

    emit themeModeChanged();
    if (effectiveTheme() != previousEffectiveTheme) {
        emit effectiveThemeChanged();
    }
}

void SettingsViewModel::setThemeColor(const QString &color)
{
    const QString sanitized = sanitizeThemeColor(color);
    if (sanitized == m_themeColor) {
        return;
    }

    m_themeColor = sanitized;
    if (m_settings) {
        m_settings->setValue(QStringLiteral("appearance/themeColor"), m_themeColor);
        m_settings->sync();
    }
    emit themeColorChanged();
}

void SettingsViewModel::setLanguageMode(const QString &mode)
{
    const QString sanitized = sanitizeLanguageMode(mode);
    if (sanitized == m_languageMode) {
        return;
    }

    m_languageMode = sanitized;
    if (m_settings) {
        m_settings->setValue(QStringLiteral("appearance/languageMode"), m_languageMode);
        m_settings->sync();
    }

    applyCurrentLanguage();
    emit languageModeChanged();
    emit languageChanged();
}

void SettingsViewModel::setMessagePayloadDisplayMode(const QString &mode)
{
    const QString sanitized = sanitizeMessagePayloadDisplayMode(mode);
    if (sanitized == m_messagePayloadDisplayMode) {
        return;
    }

    m_messagePayloadDisplayMode = sanitized;
    if (m_settings) {
        m_settings->setValue(QStringLiteral("workbench/messagePayloadDisplayMode"), m_messagePayloadDisplayMode);
        m_settings->sync();
    }
    emit messagePayloadDisplayModeChanged();
}

void SettingsViewModel::setMessageRetentionLimit(int limit)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setMessageRetentionLimit(limit);
    }
}

void SettingsViewModel::setLogRetentionLimit(int limit)
{
    if (!m_dependencies.preferencesController) {
        return;
    }

    const int previousLimit = logRetentionLimit();
    m_dependencies.preferencesController->setLogRetentionLimit(limit);
    if (logRetentionLimit() == previousLimit || logRetentionLimit() <= 0) {
        return;
    }

    if (m_dependencies.historyStore && m_dependencies.sessions) {
        for (const auto &session : *m_dependencies.sessions) {
            m_dependencies.historyStore->pruneLogs(session.id, logRetentionLimit());
        }
    }
    if (m_dependencies.reloadCurrentSessionHistory) {
        m_dependencies.reloadCurrentSessionHistory();
    }
    if (m_dependencies.emitLogStreamChanged) {
        m_dependencies.emitLogStreamChanged();
    }
}

void SettingsViewModel::setHistoryPageSize(int pageSize)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setHistoryPageSize(pageSize);
    }
}

void SettingsViewModel::setMaxIncomingPayloadBytes(int bytes)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setMaxIncomingPayloadBytes(bytes);
    }
}

void SettingsViewModel::setDeleteHistoryWithSession(bool enabled)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setDeleteHistoryWithSession(enabled);
    }
}

void SettingsViewModel::setSaveMessagesWhenOutputPaused(bool enabled)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setSaveMessagesWhenOutputPaused(enabled);
    }
}

void SettingsViewModel::setAutoCollapseConnectionListOnConnect(bool enabled)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setAutoCollapseConnectionListOnConnect(enabled);
    }
}

void SettingsViewModel::setClearMessagesOnExit(const QString &mode)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setClearMessagesOnExit(mode);
    }
}

void SettingsViewModel::setClearLogsOnExit(const QString &mode)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setClearLogsOnExit(mode);
    }
}

void SettingsViewModel::setWindowMaximized(bool maximized)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setWindowMaximized(maximized);
    }
}

void SettingsViewModel::saveWindowGeometry(int width, int height)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setWindowGeometry(width, height);
    }
}

void SettingsViewModel::saveWorkbenchLayout(
    int subscriptionPaneWidth,
    int publishComposerHeight,
    bool connectionPaneCollapsed,
    bool subscriptionPaneCollapsed)
{
    if (m_dependencies.preferencesController) {
        m_dependencies.preferencesController->setWorkbenchLayout(
            subscriptionPaneWidth,
            publishComposerHeight,
            connectionPaneCollapsed,
            subscriptionPaneCollapsed);
    }
}

void SettingsViewModel::setThemeModeIndex(int index) { setThemeMode(optionValue(SettingsOption::ThemeMode, index).toString()); }
void SettingsViewModel::setLanguageModeIndex(int index) { setLanguageMode(optionValue(SettingsOption::LanguageMode, index).toString()); }
void SettingsViewModel::setMessagePayloadDisplayModeIndex(int index) { setMessagePayloadDisplayMode(optionValue(SettingsOption::MessagePayloadDisplayMode, index).toString()); }
void SettingsViewModel::setMessageRetentionLimitIndex(int index) { setMessageRetentionLimit(optionValue(SettingsOption::MessageRetentionLimit, index).toInt()); }
void SettingsViewModel::setLogRetentionLimitIndex(int index) { setLogRetentionLimit(optionValue(SettingsOption::LogRetentionLimit, index).toInt()); }
void SettingsViewModel::setHistoryPageSizeIndex(int index) { setHistoryPageSize(optionValue(SettingsOption::HistoryPageSize, index).toInt()); }
void SettingsViewModel::setMaxIncomingPayloadBytesIndex(int index) { setMaxIncomingPayloadBytes(optionValue(SettingsOption::MaxIncomingPayloadBytes, index).toInt()); }
void SettingsViewModel::setClearMessagesOnExitIndex(int index) { setClearMessagesOnExit(optionValue(SettingsOption::CleanupMode, index).toString()); }
void SettingsViewModel::setClearLogsOnExitIndex(int index) { setClearLogsOnExit(optionValue(SettingsOption::CleanupMode, index).toString()); }

void SettingsViewModel::clearAllMessages()
{
    if (m_dependencies.historyStore) {
        m_dependencies.historyStore->clearAllMessages();
    }
    clearSessionMessages(m_dependencies.sessions);
    notifyMessagesCleared(m_dependencies);
}

void SettingsViewModel::clearAllLogs()
{
    if (m_dependencies.historyStore) {
        m_dependencies.historyStore->clearAllLogs();
    }
    clearSessionLogs(m_dependencies.sessions);
    notifyLogsCleared(m_dependencies);
}

void SettingsViewModel::clearAllHistory()
{
    if (m_dependencies.historyStore) {
        m_dependencies.historyStore->clearAllMessages();
        m_dependencies.historyStore->clearAllLogs();
    }
    clearSessionMessages(m_dependencies.sessions);
    clearSessionLogs(m_dependencies.sessions);
    notifyMessagesCleared(m_dependencies);
    notifyLogsCleared(m_dependencies);
}

void SettingsViewModel::refreshSystemColorScheme()
{
    const bool darkMode = QGuiApplication::styleHints()
        && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    if (darkMode == m_systemDarkMode) {
        return;
    }

    const QString previousEffectiveTheme = effectiveTheme();
    m_systemDarkMode = darkMode;
    if (effectiveTheme() != previousEffectiveTheme) {
        emit effectiveThemeChanged();
    }
}

QString SettingsViewModel::resolvedLanguage() const
{
    if (m_languageMode == QStringLiteral("en") || m_languageMode == QStringLiteral("zh_CN")) {
        return m_languageMode;
    }

    const QLocale systemLocale;
    return systemLocale.language() == QLocale::Chinese ? QStringLiteral("zh_CN") : QStringLiteral("en");
}

void SettingsViewModel::applyCurrentLanguage()
{
    if (m_translatorInstalled) {
        QCoreApplication::removeTranslator(&m_translator);
        m_translatorInstalled = false;
    }

    m_effectiveLanguage = resolvedLanguage();
    if (m_effectiveLanguage != QStringLiteral("zh_CN")) {
        return;
    }

    if (m_translator.load(QStringLiteral(":/i18n/mqtt_plus_zh_CN.qm"))) {
        QCoreApplication::installTranslator(&m_translator);
        m_translatorInstalled = true;
    }
}
