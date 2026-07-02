#include "viewmodels/settingsviewmodel.h"

#include "controllers/eventhistoryservice.h"
#include "controllers/preferencescontroller.h"
#include "models/eventstreammodel.h"
#include "services/apputils.h"
#include "services/storage/historystore.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QStyleHints>
#include <QVariantMap>

#include <algorithm>

using namespace AppUtils;

namespace {

QVariantList themeModeValues()
{
    return {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")};
}

QVariantList languageModeValues()
{
    return {QStringLiteral("system"), QStringLiteral("en"), QStringLiteral("zh_CN")};
}

QVariantList messageRetentionLimitValues()
{
    return {1000, 5000, 10000, 0};
}

QVariantList logRetentionLimitValues()
{
    return {500, 2000, 5000, 0};
}

QVariantList historyPageSizeValues()
{
    return {200, 500, 1000};
}

QVariantList maxIncomingPayloadByteValues()
{
    return {262144, 1048576, 5242880, 16777216};
}

QVariantList cleanupModeValues()
{
    return {QStringLiteral("never"), QStringLiteral("current"), QStringLiteral("all")};
}

int optionIndex(const QVariantList &values, const QVariant &value)
{
    for (int i = 0; i < values.size(); ++i) {
        if (values.at(i) == value) {
            return i;
        }
    }
    return 0;
}

QVariant optionValue(const QVariantList &values, int index)
{
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
        m_languageMode = sanitizeLanguageMode(
            m_settings->value(QStringLiteral("appearance/languageMode"), QStringLiteral("system")).toString());
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

    if (m_dependencies.bindMessageRetentionLimitChanged) {
        m_dependencies.bindMessageRetentionLimitChanged(this, [this]() { emit messageRetentionLimitChanged(); });
    }
    if (m_dependencies.bindLogRetentionLimitChanged) {
        m_dependencies.bindLogRetentionLimitChanged(this, [this]() { emit logRetentionLimitChanged(); });
    }
    if (m_dependencies.bindHistoryPageSizeChanged) {
        m_dependencies.bindHistoryPageSizeChanged(this, [this]() { emit historyPageSizeChanged(); });
    }
    if (m_dependencies.bindMaxIncomingPayloadBytesChanged) {
        m_dependencies.bindMaxIncomingPayloadBytesChanged(this, [this]() { emit maxIncomingPayloadBytesChanged(); });
    }
    if (m_dependencies.bindDeleteHistoryWithSessionChanged) {
        m_dependencies.bindDeleteHistoryWithSessionChanged(this, [this]() { emit deleteHistoryWithSessionChanged(); });
    }
    if (m_dependencies.bindSaveMessagesWhenOutputPausedChanged) {
        m_dependencies.bindSaveMessagesWhenOutputPausedChanged(this, [this]() { emit saveMessagesWhenOutputPausedChanged(); });
    }
    if (m_dependencies.bindClearMessagesOnExitChanged) {
        m_dependencies.bindClearMessagesOnExitChanged(this, [this]() { emit clearMessagesOnExitChanged(); });
    }
    if (m_dependencies.bindClearLogsOnExitChanged) {
        m_dependencies.bindClearLogsOnExitChanged(this, [this]() { emit clearLogsOnExitChanged(); });
    }
    if (m_dependencies.bindWindowWidthChanged) {
        m_dependencies.bindWindowWidthChanged(this, [this]() { emit windowWidthChanged(); });
    }
    if (m_dependencies.bindWindowHeightChanged) {
        m_dependencies.bindWindowHeightChanged(this, [this]() { emit windowHeightChanged(); });
    }
    if (m_dependencies.bindWindowMaximizedChanged) {
        m_dependencies.bindWindowMaximizedChanged(this, [this]() { emit windowMaximizedChanged(); });
    }
}

QString SettingsViewModel::themeMode() const { return m_themeMode; }

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
QString SettingsViewModel::clearMessagesOnExit() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->clearMessagesOnExit() : QStringLiteral("never"); }
QString SettingsViewModel::clearLogsOnExit() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->clearLogsOnExit() : QStringLiteral("never"); }
int SettingsViewModel::windowWidth() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->windowWidth() : 1480; }
int SettingsViewModel::windowHeight() const { return m_dependencies.preferencesController ? m_dependencies.preferencesController->windowHeight() : 820; }
bool SettingsViewModel::windowMaximized() const { return m_dependencies.preferencesController && m_dependencies.preferencesController->windowMaximized(); }
int SettingsViewModel::themeModeIndex() const { return optionIndex(themeModeValues(), themeMode()); }
int SettingsViewModel::languageModeIndex() const { return optionIndex(languageModeValues(), languageMode()); }
int SettingsViewModel::messageRetentionLimitIndex() const { return optionIndex(messageRetentionLimitValues(), messageRetentionLimit()); }
int SettingsViewModel::logRetentionLimitIndex() const { return optionIndex(logRetentionLimitValues(), logRetentionLimit()); }
int SettingsViewModel::historyPageSizeIndex() const { return optionIndex(historyPageSizeValues(), historyPageSize()); }
int SettingsViewModel::maxIncomingPayloadBytesIndex() const { return optionIndex(maxIncomingPayloadByteValues(), maxIncomingPayloadBytes()); }
int SettingsViewModel::clearMessagesOnExitIndex() const { return optionIndex(cleanupModeValues(), clearMessagesOnExit()); }
int SettingsViewModel::clearLogsOnExitIndex() const { return optionIndex(cleanupModeValues(), clearLogsOnExit()); }

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

void SettingsViewModel::setMessageRetentionLimit(int limit)
{
    if (!m_dependencies.preferencesController) {
        return;
    }

    const int previousLimit = messageRetentionLimit();
    m_dependencies.preferencesController->setMessageRetentionLimit(limit);
    if (messageRetentionLimit() == previousLimit || messageRetentionLimit() <= 0) {
        return;
    }

    if (m_dependencies.eventController) {
        m_dependencies.eventController->flushPendingMessageHistory();
    }
    if (m_dependencies.historyStore && m_dependencies.sessions) {
        for (const auto &session : *m_dependencies.sessions) {
            m_dependencies.historyStore->pruneMessages(session.id, messageRetentionLimit());
        }
    }
    if (m_dependencies.reloadCurrentSessionHistory) {
        m_dependencies.reloadCurrentSessionHistory();
    }
    if (m_dependencies.emitMessageStreamChanged) {
        m_dependencies.emitMessageStreamChanged();
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

void SettingsViewModel::setThemeModeIndex(int index) { setThemeMode(optionValue(themeModeValues(), index).toString()); }
void SettingsViewModel::setLanguageModeIndex(int index) { setLanguageMode(optionValue(languageModeValues(), index).toString()); }
void SettingsViewModel::setMessageRetentionLimitIndex(int index) { setMessageRetentionLimit(optionValue(messageRetentionLimitValues(), index).toInt()); }
void SettingsViewModel::setLogRetentionLimitIndex(int index) { setLogRetentionLimit(optionValue(logRetentionLimitValues(), index).toInt()); }
void SettingsViewModel::setHistoryPageSizeIndex(int index) { setHistoryPageSize(optionValue(historyPageSizeValues(), index).toInt()); }
void SettingsViewModel::setMaxIncomingPayloadBytesIndex(int index) { setMaxIncomingPayloadBytes(optionValue(maxIncomingPayloadByteValues(), index).toInt()); }
void SettingsViewModel::setClearMessagesOnExitIndex(int index) { setClearMessagesOnExit(optionValue(cleanupModeValues(), index).toString()); }
void SettingsViewModel::setClearLogsOnExitIndex(int index) { setClearLogsOnExit(optionValue(cleanupModeValues(), index).toString()); }

void SettingsViewModel::clearAllMessages()
{
    if (m_dependencies.historyStore) {
        m_dependencies.historyStore->clearAllMessages();
    }
    if (m_dependencies.sessions) {
        for (auto &session : *m_dependencies.sessions) {
            session.messageRows.clear();
            session.oldestLoadedMessageId = 0;
            session.loadedAllMessageHistory = true;
        }
    }
    if (m_dependencies.messages) {
        m_dependencies.messages->clear();
    }
    if (m_dependencies.refreshScriptTestSamplesModel) {
        m_dependencies.refreshScriptTestSamplesModel();
    }
    if (m_dependencies.emitMessageStreamChanged) {
        m_dependencies.emitMessageStreamChanged();
    }
}

void SettingsViewModel::clearAllLogs()
{
    if (m_dependencies.historyStore) {
        m_dependencies.historyStore->clearAllLogs();
    }
    if (m_dependencies.sessions) {
        for (auto &session : *m_dependencies.sessions) {
            session.logRows.clear();
            session.oldestLoadedLogId = 0;
            session.loadedAllLogHistory = true;
        }
    }
    if (m_dependencies.logs) {
        m_dependencies.logs->clear();
    }
    if (m_dependencies.emitLogStreamChanged) {
        m_dependencies.emitLogStreamChanged();
    }
}

void SettingsViewModel::clearAllHistory()
{
    if (m_dependencies.historyStore) {
        m_dependencies.historyStore->clearAllMessages();
        m_dependencies.historyStore->clearAllLogs();
    }
    if (m_dependencies.sessions) {
        for (auto &session : *m_dependencies.sessions) {
            session.messageRows.clear();
            session.oldestLoadedMessageId = 0;
            session.loadedAllMessageHistory = true;
            session.logRows.clear();
            session.oldestLoadedLogId = 0;
            session.loadedAllLogHistory = true;
        }
    }
    if (m_dependencies.messages) {
        m_dependencies.messages->clear();
    }
    if (m_dependencies.logs) {
        m_dependencies.logs->clear();
    }
    if (m_dependencies.refreshScriptTestSamplesModel) {
        m_dependencies.refreshScriptTestSamplesModel();
    }
    if (m_dependencies.emitMessageStreamChanged) {
        m_dependencies.emitMessageStreamChanged();
    }
    if (m_dependencies.emitLogStreamChanged) {
        m_dependencies.emitLogStreamChanged();
    }
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
