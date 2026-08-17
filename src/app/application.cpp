#include "app/application.h"

#include "services/apputils.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
#include <QStringList>

using namespace AppUtils;

namespace {
QString appText(const char *source)
{
    return QCoreApplication::translate("Application", source);
}
}

Application::Application()
    : m_owner(nullptr)
    , m_settings(QStringLiteral("mqtt-plus"), QStringLiteral("mqtt-plus"))
    , m_preferences(&m_settings, &m_owner)
    , m_historyWriter(new HistoryWriterWorker(
          m_historyStore.dataPath(),
          m_historyStore.nextMessageId()))
    , m_messageParser(new MessageParseWorker())
    , m_draftService(QString(), &m_owner)
    , m_sessionService(
          m_settings,
          m_historyStore,
          m_preferences,
          &m_owner)
    , m_sessionsModel(&m_owner)
    , m_subscriptionsModel(&m_owner)
    , m_filteredSubscriptionsModel(&m_owner)
    , m_messageFilterSubscriptionsModel(&m_owner)
    , m_messagesModel(&m_owner)
    , m_filteredMessagesModel(&m_owner)
    , m_logsModel(&m_owner)
    , m_processorsModel(&m_owner)
    , m_draftsModel(&m_owner)
    , m_notifications(&m_owner)
    , m_updateService(
          QCoreApplication::applicationVersion(),
          &m_owner)
    , m_updateController(
          m_settings,
          m_updateService,
          QCoreApplication::applicationVersion(),
          &m_owner)
    , m_subscriptionFpsTimer(&m_owner)
    , m_eventHistoryService(
          m_sessionService,
          m_historyStore,
          *m_historyWriter,
          *m_messageParser,
          m_messagesModel,
          m_logsModel,
          m_processorLibrary,
          timestampNow(),
          m_preferences,
          &m_owner)
    , m_subscriptionService(
          m_sessionService,
          m_eventHistoryService,
          &m_owner)
    , m_mqttService(
          m_sessionService,
          m_subscriptionService,
          m_eventHistoryService,
          &m_owner)
    , m_sessionActivityTimer(&m_owner)
    , m_viewModel(
          m_sessionService,
          m_mqttService,
          m_subscriptionService,
          m_eventHistoryService,
          m_processorLibrary,
          m_draftService,
          m_preferences,
          m_historyStore,
          m_sessionsModel,
          m_filteredSubscriptionsModel,
          m_messageFilterSubscriptionsModel,
          m_messagesModel,
          m_filteredMessagesModel,
          m_logsModel,
          m_processorsModel,
          m_draftsModel,
          m_notifications,
          m_updateController,
          m_settings,
          &m_owner)
{
    QObject::connect(
        m_viewModel.settings(),
        &SettingsViewModel::languageChanged,
        m_viewModel.updates(),
        &UpdateViewModel::retranslate);

    QObject::connect(
        m_viewModel.configurationTransfer(),
        &ConfigurationTransferService::operationFinished,
        &m_notifications,
        [this](bool success, const QString &title, const QString &message) {
            m_notifications.postOrUpdate(
                QStringLiteral("configuration-transfer"),
                title,
                message,
                success ? QStringLiteral("success") : QStringLiteral("error"),
                success ? 5000 : 0);
        });

    m_historyWriter->moveToThread(&m_historyWriterThread);
    QObject::connect(
        &m_historyWriterThread,
        &QThread::finished,
        m_historyWriter,
        &QObject::deleteLater);
    m_historyWriterThread.start();
    QMetaObject::invokeMethod(
        m_historyWriter,
        &HistoryWriterWorker::start,
        Qt::BlockingQueuedConnection);
    m_messageParser->moveToThread(&m_messageParserThread);
    QObject::connect(
        &m_messageParserThread,
        &QThread::finished,
        m_messageParser,
        &QObject::deleteLater);
    m_messageParserThread.start();
    QMetaObject::invokeMethod(
        m_messageParser,
        &MessageParseWorker::start,
        Qt::BlockingQueuedConnection);
    m_sessionService.setHistoryWriter(m_historyWriter);
    m_sessionService.setMessageParser(m_messageParser);

    m_filteredSubscriptionsModel.setSourceModel(&m_subscriptionsModel);
    m_messageFilterSubscriptionsModel.setSourceModel(&m_subscriptionsModel);
    m_filteredMessagesModel.setSourceModel(&m_messagesModel);

    QObject::connect(
        &m_mqttService,
        &MqttSessionService::sessionStateChanged,
        &m_sessionsModel,
        [this]() { refreshSessionModels(); });
    QObject::connect(
        &m_subscriptionService,
        &SubscriptionService::subscriptionsChanged,
        &m_subscriptionsModel,
        [this]() { refreshSubscriptionsModel(); });
    QObject::connect(
        &m_eventHistoryService,
        &EventHistoryService::subscriptionActivityChanged,
        &m_subscriptionsModel,
        [this]() {
            if (m_subscriptionFpsTimer.isActive()) {
                return;
            }
            refreshSubscriptionsModel();
            m_subscriptionFpsTimer.start();
        });

    QObject::connect(
        &m_sessionService,
        &SessionService::sessionRuntimeReady,
        &m_mqttService,
        &MqttSessionService::bindSessionSignals);
    QObject::connect(
        &m_sessionService,
        &SessionService::currentSessionHistoryReloadRequested,
        &m_eventHistoryService,
        &EventHistoryService::reloadCurrentSessionHistory);
    QObject::connect(
        &m_sessionService,
        &SessionService::reconnectRequested,
        &m_mqttService,
        [this](SessionState *session) {
            if (session) {
                m_mqttService.connectSession(*session, QStringLiteral("Connecting to"));
            }
        });
    QObject::connect(
        &m_sessionService,
        &SessionService::runtimeError,
        &m_eventHistoryService,
        [this](
            const QString &sessionId,
            const QString &channel,
            const QString &message) {
            if (auto *session = m_sessionService.sessionById(sessionId)) {
                m_eventHistoryService.appendEvent(*session, channel, message);
            }
        });
    QObject::connect(
        &m_sessionService,
        &SessionService::storageError,
        &m_sessionService,
        [this](const QString &message) { reportStorageError(message); });
    QObject::connect(
        &m_sessionService,
        &SessionService::sessionsChanged,
        &m_sessionsModel,
        [this]() { m_sessionsModel.setSessions(m_sessionService.sessions()); });
    QObject::connect(
        &m_sessionService,
        &SessionService::currentSessionChanged,
        &m_subscriptionsModel,
        [this]() {
            refreshSessionModels();

            if (m_subscriptionService.currentSessionHasActiveSubscriptionFps(
                    QDateTime::currentMSecsSinceEpoch())) {
                m_subscriptionFpsTimer.start();
            } else {
                m_subscriptionFpsTimer.stop();
            }
        });

    QObject::connect(
        m_viewModel.processors(),
        &ProcessorsViewModel::processorLibraryChanged,
        &m_subscriptionsModel,
        [this]() { refreshSubscriptionsModel(); });
    QObject::connect(
        m_viewModel.processors(),
        &ProcessorsViewModel::processorLibraryChanged,
        &m_eventHistoryService,
        &EventHistoryService::invalidateMessageContexts);

    QObject::connect(
        &m_draftService,
        &DraftLibraryService::draftsChanged,
        &m_draftsModel,
        [this]() { m_draftsModel.setDrafts(m_draftService.drafts()); });
    QObject::connect(
        &m_draftService,
        &DraftLibraryService::storageError,
        &m_notifications,
        [this](const QString &message) {
            m_notifications.postOrUpdate(
                QStringLiteral("draft-storage"),
                appText(QT_TRANSLATE_NOOP("Application", "Draft library error")),
                message,
                QStringLiteral("error"),
                0,
                m_draftService.canRecover()
                    ? appText(QT_TRANSLATE_NOOP("Application", "Restore backup"))
                    : QString(),
                m_draftService.canRecover() ? QStringLiteral("recoverDraftBackup") : QString());
        });
    QObject::connect(
        &m_draftService,
        &DraftLibraryService::operationSucceeded,
        &m_notifications,
        [this](const QString &operation, const QString &draftId) {
            if (operation == QStringLiteral("touch")) {
                return;
            }
            QString title;
            QString message;
            if (operation == QStringLiteral("create") || operation == QStringLiteral("update")) {
                title = appText(QT_TRANSLATE_NOOP("Application", "Draft saved"));
                if (const PublishDraft *draft = m_draftService.draftById(draftId)) {
                    message = draft->name;
                }
            } else if (operation == QStringLiteral("delete")) {
                title = appText(QT_TRANSLATE_NOOP("Application", "Draft deleted"));
                message = appText(QT_TRANSLATE_NOOP("Application", "The draft was removed from the library."));
            } else if (operation == QStringLiteral("recover")) {
                m_notifications.dismiss(QStringLiteral("draft-storage"));
                title = appText(QT_TRANSLATE_NOOP("Application", "Draft library restored"));
                message = appText(QT_TRANSLATE_NOOP("Application", "The backup was restored successfully."));
            } else {
                return;
            }
            m_notifications.postOrUpdate(
                QStringLiteral("draft-operation:%1:%2").arg(operation, draftId),
                title,
                message,
                QStringLiteral("success"),
                4000);
        });
    QObject::connect(
        &m_notifications,
        &NotificationCenterModel::actionRequested,
        &m_owner,
        [this](const QString &actionId) {
            if (actionId == QStringLiteral("recoverDraftBackup")) {
                m_draftService.recoverBackup();
            } else if (actionId == QStringLiteral("downloadUpdate")) {
                m_updateController.openDownloadPage();
            }
        });
    QObject::connect(
        &m_updateController,
        &UpdateController::checkCompleted,
        &m_notifications,
        [this](bool updateAvailable, bool userInitiated) {
            if (updateAvailable) {
                m_notifications.postOrUpdate(
                    QStringLiteral("software-update"),
                    appText(QT_TRANSLATE_NOOP("Application", "Software update available")),
                    appText(QT_TRANSLATE_NOOP("Application", "Version %1 is ready to download."))
                        .arg(m_updateController.latestVersion()),
                    QStringLiteral("info"),
                    0,
                    m_updateController.directDownloadAvailable()
                        ? appText(QT_TRANSLATE_NOOP("Application", "Download"))
                        : appText(QT_TRANSLATE_NOOP("Application", "View release")),
                    QStringLiteral("downloadUpdate"));
            } else if (userInitiated) {
                m_notifications.postOrUpdate(
                    QStringLiteral("software-update"),
                    appText(QT_TRANSLATE_NOOP("Application", "MQTT Plus is up to date")),
                    appText(QT_TRANSLATE_NOOP("Application", "You are using the latest version.")),
                    QStringLiteral("success"),
                    4000);
            }
        });
    QObject::connect(
        &m_updateController,
        &UpdateController::checkFailed,
        &m_notifications,
        [this](UpdateService::Error, bool userInitiated) {
            if (!userInitiated) {
                return;
            }
            m_notifications.postOrUpdate(
                QStringLiteral("software-update"),
                appText(QT_TRANSLATE_NOOP("Application", "Update check failed")),
                m_viewModel.updates()->statusMessage(),
                QStringLiteral("error"),
                0);
        });
    QObject::connect(
        &m_mqttService,
        &MqttSessionService::publishProgress,
        &m_notifications,
        [this](const QVariantMap &status) {
            const QString state = status.value(QStringLiteral("state")).toString();
            const int qos = status.value(QStringLiteral("qos")).toInt();
            const bool retain = status.value(QStringLiteral("retain")).toBool();
            const QString source = status.value(QStringLiteral("sourceLabel")).toString();
            const QString topic = status.value(QStringLiteral("topic")).toString();
            const QString sessionName = status.value(QStringLiteral("sessionName")).toString();
            QString title;
            QString severity = QStringLiteral("info");
            int autoCloseMs = 0;
            QString actionLabel;
            QString actionId;
            if (state == QStringLiteral("failed")) {
                title = appText(QT_TRANSLATE_NOOP("Application", "Publish failed"));
                severity = QStringLiteral("error");
                actionLabel = appText(QT_TRANSLATE_NOOP("Application", "View logs"));
                actionId = QStringLiteral("openLogs");
            } else if (state == QStringLiteral("acknowledged")
                       || state == QStringLiteral("completed")
                       || (state == QStringLiteral("sent") && qos == 0)) {
                title = appText(QT_TRANSLATE_NOOP("Application", "Message sent"));
                severity = QStringLiteral("success");
                autoCloseMs = 4000;
            } else {
                title = qos > 0
                    ? appText(QT_TRANSLATE_NOOP("Application", "Waiting for broker confirmation"))
                    : appText(QT_TRANSLATE_NOOP("Application", "Publish queued"));
            }
            QStringList parts;
            if (!source.isEmpty()) parts.append(source);
            if (!sessionName.isEmpty()) parts.append(sessionName);
            if (!topic.isEmpty()) parts.append(topic);
            parts.append(appText(QT_TRANSLATE_NOOP("Application", "QoS %1")).arg(qos));
            if (retain) {
                parts.append(appText(QT_TRANSLATE_NOOP("Application", "Retain")));
            }
            const QString reason = status.value(QStringLiteral("reason")).toString();
            if (!reason.isEmpty()) parts.append(reason);
            m_notifications.postOrUpdate(
                QStringLiteral("publish:%1").arg(status.value(QStringLiteral("requestId")).toString()),
                title,
                parts.join(QStringLiteral(" • ")),
                severity,
                autoCloseMs,
                actionLabel,
                actionId);
        });

    m_subscriptionFpsTimer.setInterval(kSubscriptionFpsRefreshIntervalMs);
    QObject::connect(
        &m_subscriptionFpsTimer,
        &QTimer::timeout,
        &m_subscriptionsModel,
        [this]() {
            const auto *session = m_sessionService.currentSession();
            if (!session
                || !m_subscriptionsModel.updateTopicFps(
                    session->subscriptions,
                    QDateTime::currentMSecsSinceEpoch())) {
                m_subscriptionFpsTimer.stop();
            }
        });

    m_sessionActivityTimer.setInterval(100);
    m_sessionActivityTimer.setSingleShot(true);
    QObject::connect(
        &m_eventHistoryService,
        &EventHistoryService::totalMessageCountChanged,
        &m_sessionActivityTimer,
        [this]() {
            if (!m_sessionActivityTimer.isActive()) {
                m_sessionActivityTimer.start();
            }
        });
    QObject::connect(
        &m_sessionActivityTimer,
        &QTimer::timeout,
        &m_sessionsModel,
        [this]() { m_sessionsModel.setSessions(m_sessionService.sessions()); });

    m_draftService.load();
    m_sessionService.loadSessions();
    applyMessageRetentionLimit();
    m_sessionService.setCurrentSessionIndex(0);
    m_updateController.scheduleAutomaticCheck();
}

Application::~Application()
{
    m_eventHistoryService.stopAcceptingIncomingMessages();
    m_eventHistoryService.flushPendingIncomingMessages();
    m_eventHistoryService.shutdownIncomingMessageAdmission();
    m_eventHistoryService.stopAcceptingMessageParsing();
    applyExitCleanup();
    QMetaObject::invokeMethod(
        m_messageParser,
        &MessageParseWorker::shutdown,
        Qt::BlockingQueuedConnection);
    m_messageParserThread.quit();
    m_messageParserThread.wait();
    m_messageParser = nullptr;

    m_historyWriter->stopAccepting();
    if (!m_historyWriter->drain()) {
        qWarning().noquote()
            << "Cannot finish draining message history writer on exit:"
            << m_historyWriter->lastError();
    }
    QMetaObject::invokeMethod(
        m_historyWriter,
        &HistoryWriterWorker::shutdown,
        Qt::BlockingQueuedConnection);
    m_historyWriterThread.quit();
    m_historyWriterThread.wait();
    m_historyWriter = nullptr;
}

ApplicationViewModel *Application::viewModel()
{
    return &m_viewModel;
}

void Application::reportStorageError(const QString &message)
{
    if (message.isEmpty()) {
        return;
    }

    SessionState *session = m_sessionService.currentSession();
    if (!session && !m_sessionService.sessions().isEmpty()) {
        session = &m_sessionService.sessions().front();
    }
    if (!session) {
        return;
    }

    session->runtime.lastError = message;
    m_eventHistoryService.appendEvent(*session, QStringLiteral("Storage"), message);
    m_sessionsModel.setSessions(m_sessionService.sessions());
    if (session == m_sessionService.currentSession()) {
        m_sessionService.currentSessionChanged();
    }
}

void Application::refreshSubscriptionsModel()
{
    const QVector<SubscriptionEntry> emptySubscriptions;
    const auto *session = m_sessionService.currentSession();
    m_subscriptionsModel.setSubscriptions(
        session ? session->id : QString(),
        session ? session->subscriptions : emptySubscriptions,
        &m_processorLibrary);
}

void Application::refreshSessionModels()
{
    m_sessionsModel.setSessions(m_sessionService.sessions());
    refreshSubscriptionsModel();
}

void Application::applyMessageRetentionLimit()
{
    const int limit = m_preferences.messageRetentionLimit();
    if (limit <= 0) {
        return;
    }

    for (const SessionState &session : m_sessionService.sessions()) {
        m_historyStore.pruneMessages(session.id, limit);
    }
}

void Application::applyExitCleanup()
{
    if (!m_eventHistoryService.flushPendingMessageHistory()) {
        qWarning().noquote()
            << "Cannot drain message history writer on exit:"
            << m_eventHistoryService.messageStorageError();
        return;
    }
    applyMessageRetentionLimit();

    const QString clearMessagesMode = m_preferences.clearMessagesOnExit();
    if (clearMessagesMode == QStringLiteral("all")) {
        if (!m_historyStore.clearAllMessages()) {
            qWarning().noquote()
                << "Cannot clear message history on exit:" << m_historyStore.lastError();
        }
    } else if (clearMessagesMode == QStringLiteral("current")) {
        if (const auto *session = m_sessionService.currentSession()) {
            if (!m_historyStore.clearMessages(session->id)) {
                qWarning().noquote()
                    << "Cannot clear current message history on exit:"
                    << m_historyStore.lastError();
            }
        }
    }

    const QString clearLogsMode = m_preferences.clearLogsOnExit();
    if (clearLogsMode == QStringLiteral("all")) {
        if (!m_historyStore.clearAllLogs()) {
            qWarning().noquote()
                << "Cannot clear log history on exit:" << m_historyStore.lastError();
        }
    } else if (clearLogsMode == QStringLiteral("current")) {
        if (const auto *session = m_sessionService.currentSession()) {
            if (!m_historyStore.clearLogs(session->id)) {
                qWarning().noquote()
                    << "Cannot clear current log history on exit:"
                    << m_historyStore.lastError();
            }
        }
    }
}
