#pragma once

#include "domain/session.h"

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTranslator>
#include <QVector>

class EventHistoryService;
class HistoryStore;
class PreferencesController;

class SettingsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString effectiveTheme READ effectiveTheme NOTIFY effectiveThemeChanged)
    Q_PROPERTY(QString themeColor READ themeColor NOTIFY themeColorChanged)
    Q_PROPERTY(int themeModeIndex READ themeModeIndex NOTIFY themeModeChanged)
    Q_PROPERTY(int languageModeIndex READ languageModeIndex NOTIFY languageModeChanged)
    Q_PROPERTY(int messagePayloadDisplayModeIndex READ messagePayloadDisplayModeIndex NOTIFY messagePayloadDisplayModeChanged)
    Q_PROPERTY(int messageRetentionLimitIndex READ messageRetentionLimitIndex NOTIFY messageRetentionLimitChanged)
    Q_PROPERTY(int logRetentionLimitIndex READ logRetentionLimitIndex NOTIFY logRetentionLimitChanged)
    Q_PROPERTY(int historyPageSizeIndex READ historyPageSizeIndex NOTIFY historyPageSizeChanged)
    Q_PROPERTY(int maxIncomingPayloadBytesIndex READ maxIncomingPayloadBytesIndex NOTIFY maxIncomingPayloadBytesChanged)
    Q_PROPERTY(int clearMessagesOnExitIndex READ clearMessagesOnExitIndex NOTIFY clearMessagesOnExitChanged)
    Q_PROPERTY(int clearLogsOnExitIndex READ clearLogsOnExitIndex NOTIFY clearLogsOnExitChanged)

public:
    explicit SettingsViewModel(
        PreferencesController &preferencesController,
        EventHistoryService &eventController,
        HistoryStore &historyStore,
        QVector<SessionState> &sessions,
        QSettings &settings,
        QObject *parent = nullptr);

    QString themeMode() const;
    QString effectiveTheme() const;
    QString themeColor() const;
    QString languageMode() const;
    int messageRetentionLimit() const;
    int logRetentionLimit() const;
    int historyPageSize() const;
    int maxIncomingPayloadBytes() const;
    QString clearMessagesOnExit() const;
    QString clearLogsOnExit() const;
    int themeModeIndex() const;
    int languageModeIndex() const;
    int messagePayloadDisplayModeIndex() const;
    int messageRetentionLimitIndex() const;
    int logRetentionLimitIndex() const;
    int historyPageSizeIndex() const;
    int maxIncomingPayloadBytesIndex() const;
    int clearMessagesOnExitIndex() const;
    int clearLogsOnExitIndex() const;

    Q_INVOKABLE void setThemeModeIndex(int index);
    Q_INVOKABLE void setThemeColor(const QString &color);
    Q_INVOKABLE void setLanguageModeIndex(int index);
    Q_INVOKABLE void setMessagePayloadDisplayModeIndex(int index);
    Q_INVOKABLE void setMessageRetentionLimitIndex(int index);
    Q_INVOKABLE void setLogRetentionLimitIndex(int index);
    Q_INVOKABLE void setHistoryPageSizeIndex(int index);
    Q_INVOKABLE void setMaxIncomingPayloadBytesIndex(int index);
    Q_INVOKABLE void setClearMessagesOnExitIndex(int index);
    Q_INVOKABLE void setClearLogsOnExitIndex(int index);

signals:
    void themeModeChanged();
    void effectiveThemeChanged();
    void themeColorChanged();
    void languageModeChanged();
    void languageChanged();
    void messagePayloadDisplayModeChanged();
    void messageRetentionLimitChanged();
    void logRetentionLimitChanged();
    void historyPageSizeChanged();
    void maxIncomingPayloadBytesChanged();
    void clearMessagesOnExitChanged();
    void clearLogsOnExitChanged();

private:
    void setThemeMode(const QString &mode);
    void setLanguageMode(const QString &mode);
    void setMessagePayloadDisplayMode(const QString &mode);
    void setLogRetentionLimit(int limit);
    void refreshSystemColorScheme();
    QString resolvedLanguage() const;
    void applyCurrentLanguage();

    QSettings &m_settings;
    PreferencesController &m_preferencesController;
    EventHistoryService &m_eventController;
    HistoryStore &m_historyStore;
    QVector<SessionState> &m_sessions;

    QString m_themeMode = QStringLiteral("system");
    QString m_themeColor = QStringLiteral("mint");
    bool m_systemDarkMode = false;

    QString m_languageMode = QStringLiteral("system");
    QString m_messagePayloadDisplayMode = QStringLiteral("hover");
    QTranslator m_translator;
    bool m_translatorInstalled = false;
};
