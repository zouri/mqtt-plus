#pragma once

#include <QObject>
#include <QVariantMap>

#include <memory>

struct ApplicationCoreState;

class ApplicationObjectGraph;

class ApplicationCore : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationCore(QObject *parent = nullptr);
    ~ApplicationCore() override;

    void notifySessionsChanged();
    void notifyCurrentSessionIndexChanged();
    void notifyCurrentSessionChanged();
    void notifySubscriptionsChanged();
    void notifyMessageStreamChanged();
    void notifyLogStreamChanged();
    void notifyMessageStreamRowAppended(const QVariantMap &row);
    void notifyLogStreamRowAppended(const QVariantMap &row);
    void notifyScriptLibraryChanged();
    void notifyThemeModeChanged();
    void notifyEffectiveThemeChanged();
    void notifyLanguageModeChanged();
    void notifyLanguageChanged();
    void notifyMessageRetentionLimitChanged();
    void notifyLogRetentionLimitChanged();
    void notifyHistoryPageSizeChanged();
    void notifyMaxIncomingPayloadBytesChanged();
    void notifyDeleteHistoryWithSessionChanged();
    void notifySaveMessagesWhenOutputPausedChanged();
    void notifyClearMessagesOnExitChanged();
    void notifyClearLogsOnExitChanged();
    void notifyWindowWidthChanged();
    void notifyWindowHeightChanged();
    void notifyWindowMaximizedChanged();

signals:
    void sessionsChanged();
    void currentSessionIndexChanged();
    void currentSessionChanged();
    void subscriptionsChanged();
    void messageStreamChanged();
    void logStreamChanged();
    void messageStreamRowAppended(const QVariantMap &row);
    void logStreamRowAppended(const QVariantMap &row);
    void scriptLibraryChanged();
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
    friend class ApplicationObjectGraph;

    std::unique_ptr<ApplicationCoreState> m_state;
};
