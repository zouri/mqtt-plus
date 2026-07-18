#pragma once

#include "domain/session.h"

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTranslator>
#include <QVariantList>
#include <QVector>

#include <functional>

class EventHistoryService;
class EventStreamModel;
class HistoryStore;
class PreferencesController;

class SettingsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString effectiveTheme READ effectiveTheme NOTIFY effectiveThemeChanged)
    Q_PROPERTY(QString themeColor READ themeColor NOTIFY themeColorChanged)
    Q_PROPERTY(QString effectiveLanguage READ effectiveLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool deleteHistoryWithSession READ deleteHistoryWithSession WRITE setDeleteHistoryWithSession NOTIFY deleteHistoryWithSessionChanged)
    Q_PROPERTY(bool saveMessagesWhenOutputPaused READ saveMessagesWhenOutputPaused WRITE setSaveMessagesWhenOutputPaused NOTIFY saveMessagesWhenOutputPausedChanged)
    Q_PROPERTY(bool autoCollapseConnectionListOnConnect READ autoCollapseConnectionListOnConnect WRITE setAutoCollapseConnectionListOnConnect NOTIFY autoCollapseConnectionListOnConnectChanged)
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY windowWidthChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY windowHeightChanged)
    Q_PROPERTY(bool windowMaximized READ windowMaximized WRITE setWindowMaximized NOTIFY windowMaximizedChanged)
    Q_PROPERTY(int subscriptionPaneWidth READ subscriptionPaneWidth CONSTANT)
    Q_PROPERTY(int publishComposerHeight READ publishComposerHeight CONSTANT)
    Q_PROPERTY(bool connectionPaneCollapsed READ connectionPaneCollapsed CONSTANT)
    Q_PROPERTY(bool subscriptionPaneCollapsed READ subscriptionPaneCollapsed CONSTANT)
    Q_PROPERTY(int themeModeIndex READ themeModeIndex NOTIFY themeModeChanged)
    Q_PROPERTY(int languageModeIndex READ languageModeIndex NOTIFY languageModeChanged)
    Q_PROPERTY(int messageRetentionLimitIndex READ messageRetentionLimitIndex NOTIFY messageRetentionLimitChanged)
    Q_PROPERTY(int logRetentionLimitIndex READ logRetentionLimitIndex NOTIFY logRetentionLimitChanged)
    Q_PROPERTY(int historyPageSizeIndex READ historyPageSizeIndex NOTIFY historyPageSizeChanged)
    Q_PROPERTY(int maxIncomingPayloadBytesIndex READ maxIncomingPayloadBytesIndex NOTIFY maxIncomingPayloadBytesChanged)
    Q_PROPERTY(int clearMessagesOnExitIndex READ clearMessagesOnExitIndex NOTIFY clearMessagesOnExitChanged)
    Q_PROPERTY(int clearLogsOnExitIndex READ clearLogsOnExitIndex NOTIFY clearLogsOnExitChanged)

public:
    struct Dependencies {
        PreferencesController *preferencesController = nullptr;
        EventHistoryService *eventController = nullptr;
        HistoryStore *historyStore = nullptr;
        QVector<SessionState> *sessions = nullptr;
        EventStreamModel *messages = nullptr;
        EventStreamModel *logs = nullptr;

        std::function<void(QObject *, std::function<void()>)> bindMessageRetentionLimitChanged;
        std::function<void(QObject *, std::function<void()>)> bindLogRetentionLimitChanged;
        std::function<void(QObject *, std::function<void()>)> bindHistoryPageSizeChanged;
        std::function<void(QObject *, std::function<void()>)> bindMaxIncomingPayloadBytesChanged;
        std::function<void(QObject *, std::function<void()>)> bindDeleteHistoryWithSessionChanged;
        std::function<void(QObject *, std::function<void()>)> bindSaveMessagesWhenOutputPausedChanged;
        std::function<void(QObject *, std::function<void()>)> bindAutoCollapseConnectionListOnConnectChanged;
        std::function<void(QObject *, std::function<void()>)> bindClearMessagesOnExitChanged;
        std::function<void(QObject *, std::function<void()>)> bindClearLogsOnExitChanged;
        std::function<void(QObject *, std::function<void()>)> bindWindowWidthChanged;
        std::function<void(QObject *, std::function<void()>)> bindWindowHeightChanged;
        std::function<void(QObject *, std::function<void()>)> bindWindowMaximizedChanged;

        std::function<void()> reloadCurrentSessionHistory;
        std::function<void()> refreshScriptTestSamplesModel;
        std::function<void()> emitMessageStreamChanged;
        std::function<void()> emitLogStreamChanged;
    };

    explicit SettingsViewModel(QSettings *settings, QObject *parent = nullptr);
    explicit SettingsViewModel(const Dependencies &dependencies, QSettings *settings, QObject *parent = nullptr);

    QString themeMode() const;
    QString effectiveTheme() const;
    QString themeColor() const;
    QString languageMode() const;
    QVariantList availableLanguages() const;
    QString effectiveLanguage() const;
    int messageRetentionLimit() const;
    int logRetentionLimit() const;
    int historyPageSize() const;
    int maxIncomingPayloadBytes() const;
    bool deleteHistoryWithSession() const;
    bool saveMessagesWhenOutputPaused() const;
    bool autoCollapseConnectionListOnConnect() const;
    QString clearMessagesOnExit() const;
    QString clearLogsOnExit() const;
    int windowWidth() const;
    int windowHeight() const;
    bool windowMaximized() const;
    int subscriptionPaneWidth() const;
    int publishComposerHeight() const;
    bool connectionPaneCollapsed() const;
    bool subscriptionPaneCollapsed() const;
    int themeModeIndex() const;
    int languageModeIndex() const;
    int messageRetentionLimitIndex() const;
    int logRetentionLimitIndex() const;
    int historyPageSizeIndex() const;
    int maxIncomingPayloadBytesIndex() const;
    int clearMessagesOnExitIndex() const;
    int clearLogsOnExitIndex() const;

    void setDeleteHistoryWithSession(bool enabled);
    void setSaveMessagesWhenOutputPaused(bool enabled);
    void setAutoCollapseConnectionListOnConnect(bool enabled);
    void setWindowMaximized(bool maximized);

    Q_INVOKABLE void saveWindowGeometry(int width, int height);
    Q_INVOKABLE void saveWorkbenchLayout(
        int subscriptionPaneWidth,
        int publishComposerHeight,
        bool connectionPaneCollapsed,
        bool subscriptionPaneCollapsed);
    Q_INVOKABLE void setThemeModeIndex(int index);
    Q_INVOKABLE void setThemeColor(const QString &color);
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
    void themeColorChanged();
    void languageModeChanged();
    void languageChanged();
    void messageRetentionLimitChanged();
    void logRetentionLimitChanged();
    void historyPageSizeChanged();
    void maxIncomingPayloadBytesChanged();
    void deleteHistoryWithSessionChanged();
    void saveMessagesWhenOutputPausedChanged();
    void autoCollapseConnectionListOnConnectChanged();
    void clearMessagesOnExitChanged();
    void clearLogsOnExitChanged();
    void windowWidthChanged();
    void windowHeightChanged();
    void windowMaximizedChanged();

private:
    void setThemeMode(const QString &mode);
    void setLanguageMode(const QString &mode);
    void setMessageRetentionLimit(int limit);
    void setLogRetentionLimit(int limit);
    void setHistoryPageSize(int pageSize);
    void setMaxIncomingPayloadBytes(int bytes);
    void setClearMessagesOnExit(const QString &mode);
    void setClearLogsOnExit(const QString &mode);
    void refreshSystemColorScheme();
    QString resolvedLanguage() const;
    void applyCurrentLanguage();

    QSettings *m_settings = nullptr;
    Dependencies m_dependencies;

    QString m_themeMode = QStringLiteral("system");
    QString m_themeColor = QStringLiteral("mint");
    bool m_systemDarkMode = false;

    QString m_languageMode = QStringLiteral("system");
    QString m_effectiveLanguage = QStringLiteral("en");
    QTranslator m_translator;
    bool m_translatorInstalled = false;
};
