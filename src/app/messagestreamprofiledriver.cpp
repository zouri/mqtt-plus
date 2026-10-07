#include "messagestreamprofiledriver.h"

#include "domain/messagecapturepolicy.h"
#include "services/payload/payloadcodec.h"
#include "services/storage/historystore.h"
#include "usecases/eventhistoryservice.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"
#include "viewmodels/applicationviewmodel.h"
#include "viewmodels/workbenchviewmodel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWheelEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryFile>

#include <algorithm>
#include <limits>
#include <utility>

#ifdef Q_OS_MACOS
#include <mach/mach.h>
#include <pthread.h>
#include <sys/resource.h>
#include <cstring>
#endif

namespace {
struct ThreadCpuSnapshot
{
    QHash<quint64, qint64> microseconds;
    QHash<quint64, QString> names;
    qint64 processMicroseconds = -1;
    bool complete = false;
};

ThreadCpuSnapshot threadCpuSnapshot()
{
    ThreadCpuSnapshot snapshot;
#ifdef Q_OS_MACOS
    thread_act_array_t threads = nullptr;
    mach_msg_type_number_t threadCount = 0;
    if (task_threads(mach_task_self(), &threads, &threadCount) != KERN_SUCCESS) {
        return snapshot;
    }
    snapshot.complete = true;
    uint64_t currentThreadId = 0;
    pthread_threadid_np(nullptr, &currentThreadId);
    for (mach_msg_type_number_t index = 0; index < threadCount; ++index) {
        thread_identifier_info_data_t identity {};
        thread_basic_info_data_t timing {};
        auto identityCount = THREAD_IDENTIFIER_INFO_COUNT;
        auto timingCount = THREAD_BASIC_INFO_COUNT;
        const bool readable = thread_info(threads[index], THREAD_IDENTIFIER_INFO,
                                  reinterpret_cast<thread_info_t>(&identity), &identityCount) == KERN_SUCCESS
            && thread_info(threads[index], THREAD_BASIC_INFO,
                   reinterpret_cast<thread_info_t>(&timing), &timingCount) == KERN_SUCCESS;
        if (readable) {
            const qint64 microseconds = qint64(timing.user_time.seconds) * 1000000
                + timing.user_time.microseconds
                + qint64(timing.system_time.seconds) * 1000000
                + timing.system_time.microseconds;
            snapshot.microseconds.insert(identity.thread_id, microseconds);
            QString name;
            thread_extended_info_data_t extended {};
            auto extendedCount = THREAD_EXTENDED_INFO_COUNT;
            if (thread_info(threads[index], THREAD_EXTENDED_INFO,
                    reinterpret_cast<thread_info_t>(&extended), &extendedCount) == KERN_SUCCESS) {
                name = QString::fromLocal8Bit(extended.pth_name,
                    strnlen(extended.pth_name, sizeof(extended.pth_name)));
            }
            if (identity.thread_id == currentThreadId) {
                name = QStringLiteral("main");
            }
            snapshot.names.insert(identity.thread_id, name);
        } else {
            snapshot.complete = false;
        }
        mach_port_deallocate(mach_task_self(), threads[index]);
    }
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(threads),
        threadCount * sizeof(thread_t));
    rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        snapshot.processMicroseconds = qint64(usage.ru_utime.tv_sec) * 1000000
            + usage.ru_utime.tv_usec + qint64(usage.ru_stime.tv_sec) * 1000000
            + usage.ru_stime.tv_usec;
    }
#endif
    return snapshot;
}
} // namespace

MessageStreamProfileDriver::MessageStreamProfileDriver(
    ApplicationViewModel &viewModel,
    MessageStreamProfileOptions options,
    QQuickWindow &window,
    QObject *parent)
    : QObject(parent)
    , m_viewModel(viewModel)
    , m_window(&window)
    , m_options(std::move(options))
{
    m_injectionTimer.setTimerType(Qt::PreciseTimer);
    m_injectionTimer.setInterval(m_options.intervalMs);
    connect(
        &m_injectionTimer,
        &QTimer::timeout,
        this,
        &MessageStreamProfileDriver::injectBatch);

    m_recoveryTimer.setInterval(50);
    connect(
        &m_recoveryTimer,
        &QTimer::timeout,
        this,
        &MessageStreamProfileDriver::waitForRecovery);

    m_scrollTimer.setTimerType(Qt::PreciseTimer);
    m_scrollTimer.setInterval(16);
    connect(&m_scrollTimer, &QTimer::timeout,
        this, &MessageStreamProfileDriver::scrollOutput);
    connect(m_window, &QWindow::activeChanged, this, [this]() {
        if (!m_finished && m_elapsed.isValid() && m_elapsed.elapsed() >= 5000
            && m_window && !m_window->isActive()) {
            m_windowLostActivationAfterWarmup = true;
        }
    });

    connect(
        m_viewModel.workbench(),
        &WorkbenchViewModel::messagePressureChanged,
        this,
        &MessageStreamProfileDriver::samplePressure);
}

void MessageStreamProfileDriver::start()
{
    if (m_started || m_finished) {
        return;
    }

    auto *sessionService = m_viewModel.sessionService();
    auto *eventHistory = m_viewModel.eventHistory();
    auto *subscriptionService = m_viewModel.subscriptionService();
    auto *workbench = m_viewModel.workbench();
    SessionState *session = sessionService ? sessionService->currentSession() : nullptr;
    if (!session || !eventHistory || !subscriptionService || !workbench) {
        qCritical().noquote() << "PROFILE_MESSAGE_STREAM unavailable: application services are not ready";
        return;
    }

    if (m_options.localBrokerPort > 0) {
        if (!m_connectionRequested) {
            m_connectionRequested = true;
            SessionConnectionConfig config;
            config.name = QStringLiteral("Isolated local profiler");
            config.host = QStringLiteral("127.0.0.1");
            config.port = m_options.localBrokerPort;
            config.clientId = SessionConfig::generateClientId();
            if (!sessionService->updateSessionConfigAt(sessionService->currentIndex(), config)
                || !session->runtime.client) {
                qCritical().noquote() << "PROFILE_MESSAGE_STREAM local connection setup failed";
                m_finished = true;
                return;
            }
            connect(session->runtime.client, &QMqttClient::connected,
                this, &MessageStreamProfileDriver::start);
            connect(session->runtime.client, &QMqttClient::disconnected, this, [this] {
                m_brokerStayedConnected = false;
            });
            QTimer::singleShot(15000, this, [this] {
                if (!m_started && !m_finished) {
                    m_finished = true;
                    qCritical().noquote() << "PROFILE_MESSAGE_STREAM local connection timed out";
                }
            });
            workbench->toggleCurrentSessionConnection();
            return;
        }
        if (!session->runtime.client
            || session->runtime.client->state() != QMqttClient::Connected) {
            return;
        }
        m_brokerStayedConnected = true;
    }
    m_started = true;
    if (m_options.observationMs > 0) {
        m_recoveryTimer.setInterval(1000);
    }

    if (!eventHistory->clearCurrentMessages()) {
        qCritical().noquote() << "PROFILE_MESSAGE_STREAM could not clear isolated history";
        m_finished = true;
        return;
    }
    sessionService->setCurrentOutputPaused(m_options.pauseOutput);
    workbench->clearMessageFilters();
    if (!sessionService->setMessageCapturePolicy(session->id, MessageCapturePolicy {})) {
        qCritical().noquote() << "PROFILE_MESSAGE_STREAM could not reset capture policy";
        m_finished = true;
        return;
    }
    if (!subscriptionService->upsertCurrentSubscription(
            QStringLiteral("profile/#"),
            0,
            static_cast<int>(PayloadFormat::Json),
            {},
            {},
            QStringLiteral("Profiler traffic"))) {
        qCritical().noquote() << "PROFILE_MESSAGE_STREAM could not persist the synthetic subscription";
        m_finished = true;
        return;
    }

    m_topics.reserve(32);
    for (int index = 0; index < 32; ++index) {
        m_topics.append(QStringLiteral("profile/topic-%1").arg(index));
    }

    const QByteArray prefix = m_options.varyPayload
        ? QByteArrayLiteral("{\"sequence\":\"0000000000\",\"value\":\"")
        : QByteArrayLiteral("{\"value\":\"");
    m_sequenceOffset = prefix.indexOf(QByteArrayLiteral("0000000000"));
    const QByteArray suffix = QByteArrayLiteral("\"}");
    const qsizetype bodyBytes = (std::max)(
        qsizetype(0),
        qsizetype(m_options.payloadBytes) - prefix.size() - suffix.size());
    m_payload = prefix + QByteArray(bodyBytes, 'x') + suffix;

    if (m_options.brokerInput) {
        m_injectionTimer.setInterval(50);
        for (const auto &entry : std::as_const(session->subscriptions)) {
            if (entry.topic != QStringLiteral("profile/#") || !entry.runtimeSubscription) {
                continue;
            }
            if (entry.runtimeSubscription->state() == QMqttSubscription::Subscribed) {
                beginObservation();
            } else {
                connect(entry.runtimeSubscription, &QMqttSubscription::stateChanged,
                    this, [this](QMqttSubscription::SubscriptionState state) {
                        if (state == QMqttSubscription::Subscribed) {
                            beginObservation();
                        }
                    });
            }
            return;
        }
        qCritical().noquote() << "PROFILE_MESSAGE_STREAM broker subscription unavailable";
        m_finished = true;
        return;
    }
    beginObservation();
}

void MessageStreamProfileDriver::beginObservation()
{
    if (m_elapsed.isValid() || m_finished) {
        return;
    }

    qInfo().noquote()
        << "PROFILE_MESSAGE_STREAM started"
        << "messages=" << m_options.messageCount
        << "batch=" << m_options.batchSize
        << "intervalMs=" << m_options.intervalMs
        << "payloadBytes=" << m_payload.size()
        << "pauseOutput=" << m_options.pauseOutput
        << "varyPayload=" << m_options.varyPayload
        << "brokerInput=" << m_options.brokerInput
        << "scrollOutput=" << m_options.scrollOutput
        << "activateWindow=" << m_options.activateWindow
        << "backgroundWindow=" << m_options.backgroundWindow;

    samplePressure();
    const auto initialThreadCpu = threadCpuSnapshot();
    m_initialThreadCpuMicroseconds = initialThreadCpu.microseconds;
    m_initialProcessCpuMicroseconds = initialThreadCpu.processMicroseconds;
    m_threadCpuAvailable = initialThreadCpu.complete;
    m_elapsed.start();
    m_windowInitiallyActive = m_window && m_window->isActive();
    if (m_options.activateWindow) {
        QTimer::singleShot(3000, this, [this]() {
            if (m_window && !m_finished) {
                m_window->requestActivate();
            }
        });
        QTimer::singleShot(5000, Qt::PreciseTimer, this, [this]()
        {
            if (!m_finished)
            {
                qInfo().noquote() << "PROFILE_MESSAGE_STREAM foregroundReady="
                                 << (m_window && m_window->isActive());
            }
        });
    }
    m_renderedFrames = 0;
    if (m_options.scrollOutput) {
        m_scrollTimer.start();
    }
    injectBatch();
    if (m_sentMessages < m_options.messageCount) {
        m_injectionTimer.start();
    }
}

void MessageStreamProfileDriver::scrollOutput()
{
    if (!m_scrollList && m_window) {
        m_scrollList = m_window->findChild<QQuickItem *>(QStringLiteral("messageEventList"));
    }
    auto *list = m_scrollList.data();
    if (!list || !list->isVisible() || list->height() <= 0
        || list->property("count").toInt() < 200
        || list->property("programmaticScroll").toBool() || m_finished) {
        return;
    }

    const qreal y = list->property("contentY").toReal();
    const qreal origin = list->property("originY").toReal();
    const qreal height = list->height();
    const qreal contentHeight = list->property("contentHeight").toReal();
    if (contentHeight < 4 * height) {
        return;
    }
    const bool manual = !list->property("shouldFollowOutput").toBool();
    if (manual) {
        ++m_manualScrollEvents;
    }
    if (m_scrollEvents > 0 && manual) {
        m_scrollDistance += qAbs(y - m_previousScrollY);
    }
    m_previousScrollY = y;
    m_scrollWasManual = m_scrollWasManual || manual;

    // Stay away from both ends: reaching the tail resumes smart follow,
    // while reaching the beginning requests another page of history.
    if ((m_scrollDirection > 0 && y <= origin + height)
        || (m_scrollDirection < 0 && y >= origin + contentHeight - 2 * height)) {
        m_scrollDirection = -m_scrollDirection;
        ++m_scrollReversals;
    }
    const QPointF position = list->mapToScene(QPointF(list->width() / 2, height / 2));
    QWheelEvent event(position, m_window->mapToGlobal(position),
        QPoint(), QPoint(0, 120 * m_scrollDirection), Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false);
    event.setTimestamp(static_cast<ulong>(m_elapsed.elapsed()));
    QCoreApplication::sendEvent(m_window, &event);
    ++m_scrollEvents;
}

void MessageStreamProfileDriver::recordRenderedFrame()
{
    if (m_started && !m_finished) {
        ++m_renderedFrames;
    }
}

void MessageStreamProfileDriver::sampleWindowActivity()
{
    if (!m_finished && m_elapsed.isValid()) {
        if (m_window && m_window->isActive()) {
            ++m_activeWindowSamples;
            if (m_elapsed.elapsed() >= 5000) {
                ++m_activeWindowSamplesAfterWarmup;
            }
        } else {
            ++m_inactiveWindowSamples;
            if (m_elapsed.elapsed() >= 5000) {
                ++m_inactiveWindowSamplesAfterWarmup;
            }
        }
    }
}

void MessageStreamProfileDriver::injectBatch()
{
    sampleWindowActivity();
    auto *sessionService = m_viewModel.sessionService();
    auto *eventHistory = m_viewModel.eventHistory();
    const SessionState *session = sessionService ? sessionService->currentSession() : nullptr;
    if (!session || !eventHistory || m_finished) {
        return;
    }

    if (m_options.brokerInput) {
        m_sentMessages = static_cast<int>((std::min)(
            m_viewModel.workbench()->totalMessageCount(),
            qint64((std::numeric_limits<int>::max)())));
        if (m_sentMessages > 0 && m_firstBrokerMessageMs < 0) {
            m_firstBrokerMessageMs = m_elapsed.elapsed();
        }
    } else {
        const int remaining = m_options.messageCount - m_sentMessages;
        const int batchCount = (std::min)(m_options.batchSize, remaining);
        for (int index = 0; index < batchCount; ++index) {
            if (m_options.varyPayload) {
                m_payload.replace(m_sequenceOffset, 10,
                    QByteArray::number(m_sentMessages).rightJustified(10, '0'));
            }
            const QString &topic = m_topics.at(m_sentMessages % m_topics.size());
            eventHistory->queueIncomingMessage(session->id, topic, m_payload);
            ++m_sentMessages;
        }
    }
    samplePressure();

    if (m_sentMessages / 1000 > m_reportedMessageGroups) {
        m_reportedMessageGroups = m_sentMessages / 1000;
        qInfo().noquote()
            << "PROFILE_MESSAGE_STREAM progress"
            << "sent=" << m_sentMessages
            << "elapsedMs=" << m_elapsed.elapsed()
            << "captured=" << m_viewModel.workbench()->totalMessageCount();
    }

    if (m_sentMessages < m_options.messageCount) {
        return;
    }

    m_injectionTimer.stop();
    m_injectionElapsedMs = m_elapsed.elapsed()
        - (m_options.brokerInput && m_firstBrokerMessageMs >= 0 ? m_firstBrokerMessageMs : 0);
    qInfo().noquote() << "PROFILE_MESSAGE_STREAM injection complete"
                     << "elapsedMs=" << m_injectionElapsedMs;
    m_recoveryTimer.start();
}

void MessageStreamProfileDriver::samplePressure()
{
    const QVariantMap pressure = m_viewModel.workbench()->messagePressure();
    m_lastPressure = pressure;
    m_maxWriterBacklog = (std::max)(
        m_maxWriterBacklog,
        pressure.value(QStringLiteral("backlog")).toInt());
    m_maxWriterBacklogBytes = (std::max)(
        m_maxWriterBacklogBytes,
        pressure.value(QStringLiteral("backlogBytes")).toLongLong());
    m_maxParserBacklog = (std::max)(
        m_maxParserBacklog,
        pressure.value(QStringLiteral("parseBacklog")).toInt());
    m_maxParserBacklogBytes = (std::max)(
        m_maxParserBacklogBytes,
        pressure.value(QStringLiteral("parseBacklogBytes")).toLongLong());

    const QString state = pressure.value(QStringLiteral("state"), QStringLiteral("normal")).toString();
    if (m_pressureStates.isEmpty() || m_pressureStates.constLast() != state) {
        m_pressureStates.append(state);
    }
}

void MessageStreamProfileDriver::waitForRecovery()
{
    sampleWindowActivity();
    samplePressure();
    const int writerBacklog = m_lastPressure.value(QStringLiteral("backlog")).toInt();
    const int parserBacklog = m_lastPressure.value(QStringLiteral("parseBacklog")).toInt();
    if (writerBacklog == 0 && parserBacklog == 0) {
        ++m_recoveryStableSamples;
    } else {
        m_recoveryStableSamples = 0;
    }

    if (m_recoveryStableSamples >= 10 && m_elapsed.elapsed() >= m_options.observationMs) {
        finish();
    }
}

void MessageStreamProfileDriver::finish()
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    const qint64 renderObservationMs = m_elapsed.elapsed();
    const auto finalThreadCpu = threadCpuSnapshot();
    const qint64 processCpuMicroseconds = m_initialProcessCpuMicroseconds >= 0
            && finalThreadCpu.processMicroseconds >= m_initialProcessCpuMicroseconds
        ? finalThreadCpu.processMicroseconds - m_initialProcessCpuMicroseconds : -1;
    QVector<QJsonObject> threadCpuRows;
    qint64 observedThreadCpuMicroseconds = 0;
    for (auto thread = finalThreadCpu.microseconds.cbegin();
         thread != finalThreadCpu.microseconds.cend(); ++thread) {
        const qint64 delta = (std::max)(qint64(0),
            thread.value() - m_initialThreadCpuMicroseconds.value(thread.key(), 0));
        observedThreadCpuMicroseconds += delta;
        threadCpuRows.append(QJsonObject {
            {QStringLiteral("name"), finalThreadCpu.names.value(thread.key())},
            {QStringLiteral("cpuMs"), double(delta) / 1000.0},
            {QStringLiteral("cpuPercent"), renderObservationMs > 0
                    ? double(delta) / (10.0 * double(renderObservationMs)) : 0.0},
        });
    }
    std::sort(threadCpuRows.begin(), threadCpuRows.end(),
        [](const QJsonObject &left, const QJsonObject &right) {
            return left.value(QStringLiteral("cpuMs")).toDouble()
                > right.value(QStringLiteral("cpuMs")).toDouble();
        });
    QJsonArray threadCpu;
    for (const auto &row : std::as_const(threadCpuRows)) {
        threadCpu.append(row);
    }
    int exitedThreads = 0;
    for (auto thread = m_initialThreadCpuMicroseconds.cbegin();
         thread != m_initialThreadCpuMicroseconds.cend(); ++thread) {
        if (!finalThreadCpu.microseconds.contains(thread.key())) {
            ++exitedThreads;
        }
    }
    m_recoveryTimer.stop();
    m_scrollTimer.stop();
    samplePressure();

    auto *workbench = m_viewModel.workbench();
    auto *messageList = m_window
        ? m_window->findChild<QQuickItem *>(QStringLiteral("messageEventList")) : nullptr;
    QQuickItem *tailItem = nullptr;
    const int lastRow = workbench->filteredMessages()->rowCount() - 1;
    const bool followTailChecked = !m_options.pauseOutput && !m_options.scrollOutput
        && messageList && lastRow >= 0
        && QMetaObject::invokeMethod(messageList, "itemAtIndex", Qt::DirectConnection,
            Q_RETURN_ARG(QQuickItem *, tailItem), Q_ARG(int, lastRow));
    const QString expectedTailId = lastRow >= 0
        ? workbench->filteredMessages()->data(
              workbench->filteredMessages()->index(lastRow, 0), EventStreamModel::HistoryIdRole).toString()
        : QString();
    const qreal tailBottom = tailItem
        ? tailItem->mapToItem(messageList, QPointF(0, tailItem->height())).y() : -1;
    const SessionState *session = m_viewModel.sessionService()->currentSession();
    HistoryStore persistedHistory;
    const qint64 persistedMessages = persistedHistory.isReady() && session
        ? persistedHistory.totalMessageCount(session->id)
        : -1;
    int persistedSuccessfulParses = 0;
    qint64 latestPersistedId = 0;
    QSet<qint64> verifiedPayloadSequences;
    if (persistedMessages == m_sentMessages) {
        const QVector<MessageRecord> records = persistedHistory.loadMessages(session->id, m_sentMessages);
        for (const MessageRecord &record : records) {
            latestPersistedId = (std::max)(latestPersistedId, record.id);
            if (record.displayState == messageParseStateName(MessageParseState::Succeeded)) {
                ++persistedSuccessfulParses;
                if (m_options.varyPayload && record.payloadSize == m_payload.size()) {
                    const QJsonObject object = QJsonDocument::fromJson(record.displayPayload.toUtf8()).object();
                    bool validSequence = false;
                    const qint64 sequence = object.value(QStringLiteral("sequence")).toString()
                                                .toLongLong(&validSequence);
                    if (validSequence && sequence >= 0 && sequence < m_sentMessages) {
                        verifiedPayloadSequences.insert(sequence);
                    }
                }
            }
        }
    }
    const bool followTailVisible = followTailChecked && tailItem && tailItem->isVisible()
        && messageList->isVisible() && messageList->height() > 0
        && latestPersistedId > 0 && expectedTailId == QString::number(latestPersistedId)
        && tailItem->property("historyId").toString() == expectedTailId
        && tailBottom > 0 && tailBottom <= messageList->height() + 2;
    bool scrollMessageVisible = false;
    if (m_options.scrollOutput && messageList && messageList->isVisible()) {
        const qreal contentY = messageList->property("contentY").toReal();
        for (const qreal fraction : {0.3, 0.5, 0.7}) {
            int index = -1;
            QQuickItem *item = nullptr;
            if (!QMetaObject::invokeMethod(messageList, "indexAt", Qt::DirectConnection,
                    Q_RETURN_ARG(int, index), Q_ARG(qreal, messageList->width() / 2),
                    Q_ARG(qreal, contentY + messageList->height() * fraction))
                || index < 0
                || !QMetaObject::invokeMethod(messageList, "itemAtIndex", Qt::DirectConnection,
                    Q_RETURN_ARG(QQuickItem *, item), Q_ARG(int, index)) || !item) {
                continue;
            }
            const QString expectedId = workbench->filteredMessages()->data(
                workbench->filteredMessages()->index(index, 0), EventStreamModel::HistoryIdRole).toString();
            const qreal bottom = item->mapToItem(messageList, QPointF(0, item->height())).y();
            scrollMessageVisible = item->isVisible() && expectedId.toLongLong() > 0
                && workbench->filteredMessages()->data(
                    workbench->filteredMessages()->index(index, 0), EventStreamModel::KindRole)
                        .toString() == QStringLiteral("message")
                && item->property("historyId").toString() == expectedId
                && bottom > 0 && bottom - item->height() < messageList->height();
            if (scrollMessageVisible) {
                break;
            }
        }
    }
    QJsonObject summary {
        {QStringLiteral("tailNativeAsciiPayload"), tailItem
             && tailItem->property("usesNativeAsciiPayload").toBool()},
        {QStringLiteral("sentMessages"), m_sentMessages},
        {QStringLiteral("windowInitiallyActive"), m_windowInitiallyActive},
        {QStringLiteral("activeWindowSamples"), m_activeWindowSamples},
        {QStringLiteral("inactiveWindowSamples"), m_inactiveWindowSamples},
        {QStringLiteral("windowFinallyActive"), m_window && m_window->isActive()},
        {QStringLiteral("activateWindow"), m_options.activateWindow},
        {QStringLiteral("backgroundWindow"), m_options.backgroundWindow},
        {QStringLiteral("windowDoesNotAcceptFocus"), m_window
            && m_window->flags().testFlag(Qt::WindowDoesNotAcceptFocus)},
        {QStringLiteral("activeWindowSamplesAfterWarmup"), m_activeWindowSamplesAfterWarmup},
        {QStringLiteral("inactiveWindowSamplesAfterWarmup"), m_inactiveWindowSamplesAfterWarmup},
        {QStringLiteral("windowLostActivationAfterWarmup"), m_windowLostActivationAfterWarmup},
        {QStringLiteral("scrollOutput"), m_options.scrollOutput},
        {QStringLiteral("scrollEvents"), m_scrollEvents},
        {QStringLiteral("manualScrollEvents"), m_manualScrollEvents},
        {QStringLiteral("scrollReversals"), m_scrollReversals},
        {QStringLiteral("scrollDistance"), m_scrollDistance},
        {QStringLiteral("scrollWasManual"), m_scrollWasManual},
        {QStringLiteral("scrollMessageVisible"), scrollMessageVisible},
        {QStringLiteral("scrollListVisible"), messageList && messageList->isVisible()
            && messageList->height() > 0},
        {QStringLiteral("scrollContentY"), messageList ? messageList->property("contentY").toReal() : 0},
        {QStringLiteral("scrollOriginY"), messageList ? messageList->property("originY").toReal() : 0},
        {QStringLiteral("scrollContentHeight"), messageList ? messageList->property("contentHeight").toReal() : 0},
        {QStringLiteral("followTailChecked"), followTailChecked},
        {QStringLiteral("followTailVisible"), followTailVisible},
        {QStringLiteral("followTailBottom"), tailBottom},
        {QStringLiteral("followViewportHeight"), messageList ? messageList->height() : 0},
        {QStringLiteral("payloadBytes"), m_payload.size()},
        {QStringLiteral("varyPayload"), m_options.varyPayload},
        {QStringLiteral("injectionElapsedMs"), m_injectionElapsedMs},
        {QStringLiteral("renderObservationMs"), renderObservationMs},
        {QStringLiteral("threadCpuAvailable"), m_threadCpuAvailable && finalThreadCpu.complete},
        {QStringLiteral("threadCpu"), threadCpu},
        {QStringLiteral("exitedObservedThreads"), exitedThreads},
        {QStringLiteral("observedThreadCpuMs"), double(observedThreadCpuMicroseconds) / 1000.0},
        {QStringLiteral("processCpuMs"), processCpuMicroseconds >= 0
                ? QJsonValue(double(processCpuMicroseconds) / 1000.0) : QJsonValue()},
        {QStringLiteral("cpuAccountingDifferenceMs"), processCpuMicroseconds >= 0
                ? QJsonValue(double(processCpuMicroseconds - observedThreadCpuMicroseconds) / 1000.0)
                : QJsonValue()},
        {QStringLiteral("renderedFrames"), m_renderedFrames},
        {QStringLiteral("renderedFramesPerSecond"),
         renderObservationMs > 0
             ? double(m_renderedFrames) * 1000.0 / double(renderObservationMs)
             : 0.0},
        {QStringLiteral("injectionMessagesPerSecond"),
         m_injectionElapsedMs > 0
             ? double(m_sentMessages) * 1000.0 / double(m_injectionElapsedMs)
             : 0.0},
        {QStringLiteral("maxWriterBacklog"), m_maxWriterBacklog},
        {QStringLiteral("maxWriterBacklogBytes"), m_maxWriterBacklogBytes},
        {QStringLiteral("maxParserBacklog"), m_maxParserBacklog},
        {QStringLiteral("maxParserBacklogBytes"), m_maxParserBacklogBytes},
        {QStringLiteral("droppedMessages"),
         m_lastPressure.value(QStringLiteral("dropped")).toLongLong()},
        {QStringLiteral("droppedParseTasks"),
         m_lastPressure.value(QStringLiteral("parseDropped")).toLongLong()},
        {QStringLiteral("droppedParseResults"),
         m_lastPressure.value(QStringLiteral("parseResultDropped")).toLongLong()},
        {QStringLiteral("pressureSkippedParses"),
         m_lastPressure.value(QStringLiteral("parseSkippedPressure")).toLongLong()},
        {QStringLiteral("totalMessageCount"), workbench->totalMessageCount()},
        {QStringLiteral("persistedMessageCount"), persistedMessages},
        {QStringLiteral("persistedSuccessfulParses"), persistedSuccessfulParses},
        {QStringLiteral("verifiedPayloadSequences"), verifiedPayloadSequences.size()},
        {QStringLiteral("visibleMessageCount"), workbench->filteredMessages()->rowCount()},
        {QStringLiteral("finalState"),
         m_lastPressure.value(QStringLiteral("state"), QStringLiteral("normal")).toString()},
        {QStringLiteral("pressureStates"), QJsonArray::fromStringList(m_pressureStates)},
    };
    const auto *profileSession = m_viewModel.sessionService()->currentSession();
    summary.insert(QStringLiteral("localBrokerPort"), m_options.localBrokerPort);
    summary.insert(QStringLiteral("brokerInput"), m_options.brokerInput);
    summary.insert(QStringLiteral("brokerStayedConnected"), m_brokerStayedConnected);
    summary.insert(QStringLiteral("brokerConnected"), profileSession
        && profileSession->runtime.client
        && profileSession->runtime.client->state() == QMqttClient::Connected);
    if (m_options.captureWindow)
    {
        QTemporaryFile file(QDir::temp().filePath(QStringLiteral("mqtt-profile-window-XXXXXX.png")));
        const QImage image = m_window ? m_window->grabWindow() : QImage();
        const bool saved = !image.isNull() && file.open() && image.save(&file, "PNG") && file.flush();
        if (saved)
        {
            file.setAutoRemove(false);
            summary.insert(QStringLiteral("windowImageFile"), QFileInfo(file.fileName()).fileName());
            summary.insert(QStringLiteral("windowImageWidth"), image.width());
            summary.insert(QStringLiteral("windowImageHeight"), image.height());
        }
        summary.insert(QStringLiteral("windowImageCaptured"), saved);
        qInfo().noquote() << "PROFILE_MESSAGE_STREAM windowImageCaptured=" << saved;
    }
    qInfo().noquote()
        << "PROFILE_MESSAGE_STREAM summary"
        << QJsonDocument(summary).toJson(QJsonDocument::Compact);
}
