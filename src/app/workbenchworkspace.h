#pragma once

#include "platform/platformactions.h"
#include "app/workbenchworkspacedependencies.h"
#include "viewmodels/workbenchcoreport.h"

class WorkbenchWorkspace : public WorkbenchCorePort
{
public:
    explicit WorkbenchWorkspace(const WorkbenchWorkspaceDependencies &dependencies = {});

    void bindWorkbenchSignals(QObject *context, const WorkbenchCoreSignalHandlers &handlers) override;

    SessionListModel *sessions() override;
    SubscriptionFilterModel *filteredSubscriptions() override;
    EventStreamModel *messages() override;
    ScriptLibraryModel *scripts() override;
    int currentSessionIndex() const override;
    QVariantMap currentSession() const override;
    QVariantMap sessionStatus() const override;
    QVariantMap publishStatus() const override;
    QStringList payloadFormats() const override;

    void setCurrentSessionIndex(int index) override;
    QVariantMap defaultSessionConfig() const override;
    QVariantMap sessionConfigAt(int index) const override;
    bool updateSessionConfigAt(int index, const QVariantMap &config) override;
    void addSessionWithConfig(const QVariantMap &config) override;
    void duplicateSessionAt(int index) override;
    void removeSessionAt(int index) override;
    QString showSessionContextMenu(int index, const QPointF &globalPosition) override;
    QString showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition) override;

    void connectCurrentSession() override;
    void disconnectCurrentSession() override;
    void setCurrentOutputPaused(bool paused) override;
    bool upsertCurrentSubscription(
        const QString &topic,
        int qos = 0,
        int format = 0,
        const QString &scriptId = QString(),
        const QString &alias = QString()) override;
    bool updateCurrentSubscription(
        const QString &topic,
        const QString &newTopic,
        const QString &alias,
        const QString &scriptId) override;
    void removeCurrentSubscription(const QString &topic) override;
    void setCurrentSubscriptionPaused(const QString &topic, bool paused) override;
    void publishCurrentSession(
        const QString &topic,
        const QString &payload,
        int format = 0,
        int qos = 0,
        bool retain = false) override;
    void copyTextToClipboard(const QString &text) const override;
    void clearCurrentMessages() override;
    int loadOlderCurrentSessionMessages() override;

private:
    WorkbenchWorkspaceDependencies m_dependencies;
    PlatformActions m_platformActions;
};
