#pragma once

#include <functional>

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class EventStreamModel;
class QObject;
class ScriptLibraryModel;
class SessionListModel;
class SubscriptionFilterModel;

struct WorkbenchCoreSignalHandlers
{
    std::function<void()> currentSessionIndexChanged;
    std::function<void()> currentSessionChanged;
    std::function<void()> messageStreamChanged;
    std::function<void()> messageStreamRowAppended;
    std::function<void()> scriptLibraryChanged;
};

class WorkbenchCorePort
{
public:
    virtual ~WorkbenchCorePort() = default;

    virtual void bindWorkbenchSignals(QObject *context, const WorkbenchCoreSignalHandlers &handlers) = 0;

    virtual SessionListModel *sessions() = 0;
    virtual SubscriptionFilterModel *filteredSubscriptions() = 0;
    virtual EventStreamModel *messages() = 0;
    virtual ScriptLibraryModel *scripts() = 0;
    virtual int currentSessionIndex() const = 0;
    virtual QVariantMap currentSession() const = 0;
    virtual QVariantMap sessionStatus() const = 0;
    virtual QVariantMap publishStatus() const = 0;
    virtual QStringList payloadFormats() const = 0;

    virtual void setCurrentSessionIndex(int index) = 0;
    virtual QVariantMap defaultSessionConfig() const = 0;
    virtual QVariantMap sessionConfigAt(int index) const = 0;
    virtual bool updateSessionConfigAt(int index, const QVariantMap &config) = 0;
    virtual void addSessionWithConfig(const QVariantMap &config) = 0;
    virtual void duplicateSessionAt(int index) = 0;
    virtual void removeSessionAt(int index) = 0;
    virtual QString showSessionContextMenu(int index, const QPointF &globalPosition) = 0;
    virtual QString showSubscriptionContextMenu(const QString &topic, const QPointF &globalPosition) = 0;

    virtual void connectCurrentSession() = 0;
    virtual void disconnectCurrentSession() = 0;
    virtual void setCurrentOutputPaused(bool paused) = 0;
    virtual bool upsertCurrentSubscription(
        const QString &topic,
        int qos = 0,
        int format = 0,
        const QString &scriptId = QString(),
        const QString &alias = QString()) = 0;
    virtual bool updateCurrentSubscription(
        const QString &topic,
        const QString &newTopic,
        const QString &alias,
        const QString &scriptId) = 0;
    virtual void removeCurrentSubscription(const QString &topic) = 0;
    virtual void setCurrentSubscriptionPaused(const QString &topic, bool paused) = 0;
    virtual void publishCurrentSession(
        const QString &topic,
        const QString &payload,
        int format = 0,
        int qos = 0,
        bool retain = false) = 0;
    virtual void copyTextToClipboard(const QString &text) const = 0;
    virtual void clearCurrentMessages() = 0;
    virtual int loadOlderCurrentSessionMessages() = 0;
};
