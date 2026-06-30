#pragma once

#include "viewmodels/settingsoptionsviewmodel.h"

#include <QString>
#include <QVariantList>

class ApplicationCore;

class SettingsViewModel : public SettingsOptionsViewModel
{
    Q_OBJECT
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString effectiveTheme READ effectiveTheme NOTIFY effectiveThemeChanged)
    Q_PROPERTY(QString languageMode READ languageMode WRITE setLanguageMode NOTIFY languageModeChanged)
    Q_PROPERTY(QString effectiveLanguage READ effectiveLanguage NOTIFY languageChanged)
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages NOTIFY languageChanged)
    Q_PROPERTY(int messageRetentionLimit READ messageRetentionLimit WRITE setMessageRetentionLimit NOTIFY messageRetentionLimitChanged)
    Q_PROPERTY(int logRetentionLimit READ logRetentionLimit WRITE setLogRetentionLimit NOTIFY logRetentionLimitChanged)
    Q_PROPERTY(int historyPageSize READ historyPageSize WRITE setHistoryPageSize NOTIFY historyPageSizeChanged)
    Q_PROPERTY(int maxIncomingPayloadBytes READ maxIncomingPayloadBytes WRITE setMaxIncomingPayloadBytes NOTIFY maxIncomingPayloadBytesChanged)
    Q_PROPERTY(bool deleteHistoryWithSession READ deleteHistoryWithSession WRITE setDeleteHistoryWithSession NOTIFY deleteHistoryWithSessionChanged)
    Q_PROPERTY(bool saveMessagesWhenOutputPaused READ saveMessagesWhenOutputPaused WRITE setSaveMessagesWhenOutputPaused NOTIFY saveMessagesWhenOutputPausedChanged)
    Q_PROPERTY(QString clearMessagesOnExit READ clearMessagesOnExit WRITE setClearMessagesOnExit NOTIFY clearMessagesOnExitChanged)
    Q_PROPERTY(QString clearLogsOnExit READ clearLogsOnExit WRITE setClearLogsOnExit NOTIFY clearLogsOnExitChanged)
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY windowHeightChanged)
    Q_PROPERTY(bool windowMaximized READ windowMaximized WRITE setWindowMaximized NOTIFY windowMaximizedChanged)
    Q_PROPERTY(int themeModeIndex READ themeModeIndex NOTIFY themeModeChanged)
    Q_PROPERTY(int languageModeIndex READ languageModeIndex NOTIFY languageModeChanged)
    Q_PROPERTY(int messageRetentionLimitIndex READ messageRetentionLimitIndex NOTIFY messageRetentionLimitChanged)
    Q_PROPERTY(int logRetentionLimitIndex READ logRetentionLimitIndex NOTIFY logRetentionLimitChanged)
    Q_PROPERTY(int historyPageSizeIndex READ historyPageSizeIndex NOTIFY historyPageSizeChanged)
    Q_PROPERTY(int maxIncomingPayloadBytesIndex READ maxIncomingPayloadBytesIndex NOTIFY maxIncomingPayloadBytesChanged)
    Q_PROPERTY(int clearMessagesOnExitIndex READ clearMessagesOnExitIndex NOTIFY clearMessagesOnExitChanged)
    Q_PROPERTY(int clearLogsOnExitIndex READ clearLogsOnExitIndex NOTIFY clearLogsOnExitChanged)

public:
    explicit SettingsViewModel(ApplicationCore *core = nullptr, QObject *parent = nullptr);

    QString themeMode() const;
    QString effectiveTheme() const;
    QString languageMode() const;
    QString effectiveLanguage() const;
    QVariantList availableLanguages() const;
    int messageRetentionLimit() const;
    int logRetentionLimit() const;
    int historyPageSize() const;
    int maxIncomingPayloadBytes() const;
    bool deleteHistoryWithSession() const;
    bool saveMessagesWhenOutputPaused() const;
    QString clearMessagesOnExit() const;
    QString clearLogsOnExit() const;
    int windowWidth() const;
    int windowHeight() const;
    bool windowMaximized() const;
    int themeModeIndex() const;
    int languageModeIndex() const;
    int messageRetentionLimitIndex() const;
    int logRetentionLimitIndex() const;
    int historyPageSizeIndex() const;
    int maxIncomingPayloadBytesIndex() const;
    int clearMessagesOnExitIndex() const;
    int clearLogsOnExitIndex() const;

    void setThemeMode(const QString &mode);
    void setLanguageMode(const QString &mode);
    void setMessageRetentionLimit(int limit);
    void setLogRetentionLimit(int limit);
    void setHistoryPageSize(int pageSize);
    void setMaxIncomingPayloadBytes(int bytes);
    void setDeleteHistoryWithSession(bool enabled);
    void setSaveMessagesWhenOutputPaused(bool enabled);
    void setClearMessagesOnExit(const QString &mode);
    void setClearLogsOnExit(const QString &mode);
    void setWindowMaximized(bool maximized);

    Q_INVOKABLE void saveWindowGeometry(int width, int height);
    Q_INVOKABLE void setThemeModeIndex(int index);
    Q_INVOKABLE void setLanguageModeIndex(int index);
    Q_INVOKABLE void setMessageRetentionLimitIndex(int index);
    Q_INVOKABLE void setLogRetentionLimitIndex(int index);
    Q_INVOKABLE void setHistoryPageSizeIndex(int index);
    Q_INVOKABLE void setMaxIncomingPayloadBytesIndex(int index);
    Q_INVOKABLE void setClearMessagesOnExitIndex(int index);
    Q_INVOKABLE void setClearLogsOnExitIndex(int index);
    Q_INVOKABLE void clearAllMessages();
    Q_INVOKABLE void clearAllLogs();
    Q_INVOKABLE void clearAllHistory();

signals:
    void themeModeChanged();
    void effectiveThemeChanged();
    void languageModeChanged();
    void languageChanged();
    void messageRetentionLimitChanged();
    void logRetentionLimitChanged();
    void historyPageSizeChanged();
    void maxIncomingPayloadBytesChanged();
    void deleteHistoryWithSessionChanged();
    void saveMessagesWhenOutputPausedChanged();
    void clearMessagesOnExitChanged();
    void clearLogsOnExitChanged();
    void windowWidthChanged();
    void windowHeightChanged();
    void windowMaximizedChanged();

private:
    ApplicationCore *m_core = nullptr;
};
