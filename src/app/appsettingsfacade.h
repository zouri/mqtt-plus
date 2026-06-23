#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <functional>

class EventStreamModel;
class HistoryStore;
class LanguageController;
class PreferencesController;
class SessionController;
class ThemeController;

class AppSettingsFacade : public QObject
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
    Q_PROPERTY(bool deleteHistoryWithSession READ deleteHistoryWithSession WRITE setDeleteHistoryWithSession NOTIFY deleteHistoryWithSessionChanged)
    Q_PROPERTY(bool saveMessagesWhenOutputPaused READ saveMessagesWhenOutputPaused WRITE setSaveMessagesWhenOutputPaused NOTIFY saveMessagesWhenOutputPausedChanged)
    Q_PROPERTY(QString clearMessagesOnExit READ clearMessagesOnExit WRITE setClearMessagesOnExit NOTIFY clearMessagesOnExitChanged)
    Q_PROPERTY(QString clearLogsOnExit READ clearLogsOnExit WRITE setClearLogsOnExit NOTIFY clearLogsOnExitChanged)
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY windowHeightChanged)
    Q_PROPERTY(bool windowMaximized READ windowMaximized WRITE setWindowMaximized NOTIFY windowMaximizedChanged)

public:
    struct Dependencies
    {
        ThemeController *themeController = nullptr;
        LanguageController *languageController = nullptr;
        PreferencesController *preferencesController = nullptr;
        SessionController *sessionController = nullptr;
        HistoryStore *historyStore = nullptr;
        EventStreamModel *messagesModel = nullptr;
        EventStreamModel *logsModel = nullptr;
        std::function<void()> flushPendingMessageHistory;
        std::function<void()> reloadCurrentSessionHistory;
        std::function<void()> refreshScriptTestSamplesModel;
        std::function<void()> emitMessageStreamChanged;
        std::function<void()> emitLogStreamChanged;
        std::function<void()> emitScriptTestSamplesChanged;
    };

    explicit AppSettingsFacade(Dependencies dependencies, QObject *parent = nullptr);

    QString themeMode() const;
    QString effectiveTheme() const;
    QString languageMode() const;
    QString effectiveLanguage() const;
    QVariantList availableLanguages() const;
    int messageRetentionLimit() const;
    int logRetentionLimit() const;
    int historyPageSize() const;
    bool deleteHistoryWithSession() const;
    bool saveMessagesWhenOutputPaused() const;
    QString clearMessagesOnExit() const;
    QString clearLogsOnExit() const;
    int windowWidth() const;
    int windowHeight() const;
    bool windowMaximized() const;

    void setThemeMode(const QString &mode);
    void setLanguageMode(const QString &mode);
    void setMessageRetentionLimit(int limit);
    void setLogRetentionLimit(int limit);
    void setHistoryPageSize(int pageSize);
    void setDeleteHistoryWithSession(bool enabled);
    void setSaveMessagesWhenOutputPaused(bool enabled);
    void setClearMessagesOnExit(const QString &mode);
    void setClearLogsOnExit(const QString &mode);
    void setWindowMaximized(bool maximized);

    Q_INVOKABLE void saveWindowGeometry(int width, int height);
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
    void deleteHistoryWithSessionChanged();
    void saveMessagesWhenOutputPausedChanged();
    void clearMessagesOnExitChanged();
    void clearLogsOnExitChanged();
    void windowWidthChanged();
    void windowHeightChanged();
    void windowMaximizedChanged();

private:
    Dependencies m_dependencies;
};
