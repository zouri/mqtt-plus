#include "viewmodels/settingsviewmodel.h"

#include "usecases/eventhistoryservice.h"
#include "usecases/preferencescontroller.h"
#include "services/apputils.h"
#include "services/storage/historystore.h"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLocale>
#include <QStyleHints>
#include <QStringList>
#include <QVariantList>

#include <algorithm>
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

QStringList availableFixedFontFamilies()
{
    QStringList fixedFontFamilies;
    const QStringList families = QFontDatabase::families();
    for (const QString &family : families) {
        if (!QFontDatabase::isPrivateFamily(family)
            && QFontDatabase::isFixedPitch(family)) {
            fixedFontFamilies.append(family);
        }
    }
    fixedFontFamilies.sort(Qt::CaseInsensitive);
    return fixedFontFamilies;
}

QString preferredFixedFontFamily(const QStringList &fixedFontFamilies)
{
    const QString platformFixedFamily =
        QFontDatabase::systemFont(QFontDatabase::FixedFont).defaultFamily();
    if (fixedFontFamilies.contains(platformFixedFamily)) {
        return platformFixedFamily;
    }

#if defined(Q_OS_MACOS)
    const QStringList preferredFamilies {
        QStringLiteral("SF Mono"),
        QStringLiteral("Menlo"),
        QStringLiteral("Monaco"),
    };
#elif defined(Q_OS_WIN)
    const QStringList preferredFamilies {
        QStringLiteral("Cascadia Mono"),
        QStringLiteral("Consolas"),
        QStringLiteral("Courier New"),
    };
#else
    const QStringList preferredFamilies {
        QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("Noto Sans Mono"),
        QStringLiteral("Liberation Mono"),
    };
#endif

    for (const QString &family : preferredFamilies) {
        if (fixedFontFamilies.contains(family)) {
            return family;
        }
    }

    return fixedFontFamilies.value(0, platformFixedFamily);
}

} // namespace

SettingsViewModel::SettingsViewModel(
    PreferencesController &preferencesController,
    EventHistoryService &eventController,
    HistoryStore &historyStore,
    QVector<SessionState> &sessions,
    QSettings &settings,
    QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_preferencesController(preferencesController)
    , m_eventController(eventController)
    , m_historyStore(historyStore)
    , m_sessions(sessions)
{
    m_themeMode = sanitizeThemeMode(
        m_settings.value(QStringLiteral("appearance/themeMode"), QStringLiteral("system")).toString());
    m_themeColor = sanitizeThemeColor(
        m_settings.value(QStringLiteral("appearance/themeColor"), QStringLiteral("mint")).toString());
    m_animationsEnabled =
        m_settings.value(QStringLiteral("appearance/animationsEnabled"), m_animationsEnabled).toBool();
    m_availableFontFamilies = availableFixedFontFamilies();
    const QString storedFontFamily =
        m_settings.value(QStringLiteral("appearance/fontFamily")).toString().trimmed();
    if (m_availableFontFamilies.contains(storedFontFamily)) {
        m_fontFamily = storedFontFamily;
    } else {
        m_fontFamily = preferredFixedFontFamily(m_availableFontFamilies);
    }
    m_languageMode = sanitizeLanguageMode(
        m_settings.value(QStringLiteral("appearance/languageMode"), QStringLiteral("system")).toString());
    m_messagePayloadDisplayMode = sanitizeMessagePayloadDisplayMode(
        m_settings.value(QStringLiteral("workbench/messagePayloadDisplayMode"), QStringLiteral("hover")).toString());
    refreshSystemColorScheme();
    applyCurrentLanguage();

    if (QGuiApplication::styleHints()) {
        connect(
            QGuiApplication::styleHints(),
            &QStyleHints::colorSchemeChanged,
            this,
            [this](Qt::ColorScheme) { refreshSystemColorScheme(); });
    }

    connect(&m_preferencesController, &PreferencesController::messageRetentionLimitChanged, this, &SettingsViewModel::messageRetentionLimitChanged);
    connect(&m_preferencesController, &PreferencesController::logRetentionLimitChanged, this, &SettingsViewModel::logRetentionLimitChanged);
    connect(&m_preferencesController, &PreferencesController::historyPageSizeChanged, this, &SettingsViewModel::historyPageSizeChanged);
    connect(&m_preferencesController, &PreferencesController::maxIncomingPayloadBytesChanged, this, &SettingsViewModel::maxIncomingPayloadBytesChanged);
    connect(&m_preferencesController, &PreferencesController::clearMessagesOnExitChanged, this, &SettingsViewModel::clearMessagesOnExitChanged);
    connect(&m_preferencesController, &PreferencesController::clearLogsOnExitChanged, this, &SettingsViewModel::clearLogsOnExitChanged);
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

bool SettingsViewModel::animationsEnabled() const { return m_animationsEnabled; }

QString SettingsViewModel::effectiveFontFamily() const
{
    return m_fontFamily;
}

QStringList SettingsViewModel::availableFontFamilies() const { return m_availableFontFamilies; }

int SettingsViewModel::fontFamilyIndex() const
{
    return m_availableFontFamilies.indexOf(m_fontFamily);
}

QString SettingsViewModel::languageMode() const { return m_languageMode; }

int SettingsViewModel::messageRetentionLimit() const { return m_preferencesController.messageRetentionLimit(); }
int SettingsViewModel::logRetentionLimit() const { return m_preferencesController.logRetentionLimit(); }
int SettingsViewModel::historyPageSize() const { return m_preferencesController.historyPageSize(); }
int SettingsViewModel::maxIncomingPayloadBytes() const { return m_preferencesController.maxIncomingPayloadBytes(); }
QString SettingsViewModel::clearMessagesOnExit() const { return m_preferencesController.clearMessagesOnExit(); }
QString SettingsViewModel::clearLogsOnExit() const { return m_preferencesController.clearLogsOnExit(); }
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
    m_settings.setValue(QStringLiteral("appearance/themeMode"), m_themeMode);
    m_settings.sync();

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
    m_settings.setValue(QStringLiteral("appearance/themeColor"), m_themeColor);
    m_settings.sync();
    emit themeColorChanged();
}

void SettingsViewModel::setAnimationsEnabled(bool enabled)
{
    if (enabled == m_animationsEnabled) {
        return;
    }

    m_animationsEnabled = enabled;
    m_settings.setValue(QStringLiteral("appearance/animationsEnabled"), m_animationsEnabled);
    m_settings.sync();
    emit animationsEnabledChanged();
}

void SettingsViewModel::setFontFamilyIndex(int index)
{
    if (index < 0 || index >= m_availableFontFamilies.size()) {
        return;
    }

    const QString family = m_availableFontFamilies.at(index);
    if (family == m_fontFamily) {
        return;
    }

    m_fontFamily = family;
    m_settings.setValue(QStringLiteral("appearance/fontFamily"), m_fontFamily);
    m_settings.sync();
    emit fontFamilyChanged();
}

void SettingsViewModel::setLanguageMode(const QString &mode)
{
    const QString sanitized = sanitizeLanguageMode(mode);
    if (sanitized == m_languageMode) {
        return;
    }

    m_languageMode = sanitized;
    m_settings.setValue(QStringLiteral("appearance/languageMode"), m_languageMode);
    m_settings.sync();

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
    m_settings.setValue(QStringLiteral("workbench/messagePayloadDisplayMode"), m_messagePayloadDisplayMode);
    m_settings.sync();
    emit messagePayloadDisplayModeChanged();
}

void SettingsViewModel::setLogRetentionLimit(int limit)
{
    const int previousLimit = logRetentionLimit();
    m_preferencesController.setLogRetentionLimit(limit);
    if (logRetentionLimit() == previousLimit || logRetentionLimit() <= 0) {
        return;
    }

    pruneLogsToCurrentLimit();
}

void SettingsViewModel::reloadPortableSettings(bool logRetentionLimitChanged)
{
    setThemeMode(
        m_settings.value(QStringLiteral("appearance/themeMode"), QStringLiteral("system"))
            .toString());
    setThemeColor(
        m_settings.value(QStringLiteral("appearance/themeColor"), QStringLiteral("mint"))
            .toString());
    setAnimationsEnabled(
        m_settings.value(QStringLiteral("appearance/animationsEnabled"), true).toBool());
    setLanguageMode(
        m_settings.value(QStringLiteral("appearance/languageMode"), QStringLiteral("system"))
            .toString());
    setMessagePayloadDisplayMode(
        m_settings.value(
                      QStringLiteral("workbench/messagePayloadDisplayMode"),
                      QStringLiteral("hover"))
            .toString());
    if (logRetentionLimitChanged && logRetentionLimit() > 0) {
        pruneLogsToCurrentLimit();
    }
}

void SettingsViewModel::pruneLogsToCurrentLimit()
{
    for (const auto &session : m_sessions) {
        m_historyStore.pruneLogs(session.id, logRetentionLimit());
    }
    m_eventController.reloadCurrentSessionHistory();
    emit m_eventController.messageStreamChanged();
    emit m_eventController.logStreamChanged();
}

void SettingsViewModel::setThemeModeIndex(int index) { setThemeMode(optionValue(SettingsOption::ThemeMode, index).toString()); }
void SettingsViewModel::setLanguageModeIndex(int index) { setLanguageMode(optionValue(SettingsOption::LanguageMode, index).toString()); }
void SettingsViewModel::setMessagePayloadDisplayModeIndex(int index) { setMessagePayloadDisplayMode(optionValue(SettingsOption::MessagePayloadDisplayMode, index).toString()); }
void SettingsViewModel::setMessageRetentionLimitIndex(int index) { m_preferencesController.setMessageRetentionLimit(optionValue(SettingsOption::MessageRetentionLimit, index).toInt()); }
void SettingsViewModel::setLogRetentionLimitIndex(int index) { setLogRetentionLimit(optionValue(SettingsOption::LogRetentionLimit, index).toInt()); }
void SettingsViewModel::setHistoryPageSizeIndex(int index) { m_preferencesController.setHistoryPageSize(optionValue(SettingsOption::HistoryPageSize, index).toInt()); }
void SettingsViewModel::setMaxIncomingPayloadBytesIndex(int index) { m_preferencesController.setMaxIncomingPayloadBytes(optionValue(SettingsOption::MaxIncomingPayloadBytes, index).toInt()); }
void SettingsViewModel::setClearMessagesOnExitIndex(int index) { m_preferencesController.setClearMessagesOnExit(optionValue(SettingsOption::CleanupMode, index).toString()); }
void SettingsViewModel::setClearLogsOnExitIndex(int index) { m_preferencesController.setClearLogsOnExit(optionValue(SettingsOption::CleanupMode, index).toString()); }

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

    const QString language = resolvedLanguage();
    if (language != QStringLiteral("zh_CN")) {
        return;
    }

    if (m_translator.load(QStringLiteral(":/i18n/mqtt_plus_zh_CN.qm"))) {
        QCoreApplication::installTranslator(&m_translator);
        m_translatorInstalled = true;
    }
}
