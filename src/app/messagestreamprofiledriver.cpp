#include "messagestreamprofiledriver.h"

#include "services/messaging/messagecapturepolicy.h"
#include "services/payload/payloadcodec.h"
#include "usecases/eventhistoryservice.h"
#include "usecases/sessionservice.h"
#include "usecases/subscriptionservice.h"
#include "viewmodels/applicationviewmodel.h"
#include "viewmodels/workbenchviewmodel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <utility>

MessageStreamProfileDriver::MessageStreamProfileDriver(
    ApplicationViewModel &viewModel,
    MessageStreamProfileOptions options,
    QObject *parent)
    : QObject(parent)
    , m_viewModel(viewModel)
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

    connect(
        m_viewModel.workbench(),
        &WorkbenchViewModel::messagePressureChanged,
        this,
        &MessageStreamProfileDriver::samplePressure);
}

void MessageStreamProfileDriver::start()
{
    if (m_started) {
        return;
    }
    m_started = true;

    auto *sessionService = m_viewModel.sessionService();
    auto *eventHistory = m_viewModel.eventHistory();
    auto *subscriptionService = m_viewModel.subscriptionService();
    auto *workbench = m_viewModel.workbench();
    SessionState *session = sessionService ? sessionService->currentSession() : nullptr;
    if (!session || !eventHistory || !subscriptionService || !workbench) {
        qCritical().noquote() << "PROFILE_MESSAGE_STREAM unavailable: application services are not ready";
        return;
    }

    if (!eventHistory->clearCurrentMessages()) {
        qCritical().noquote() << "PROFILE_MESSAGE_STREAM could not clear isolated history";
        m_finished = true;
        return;
    }
    sessionService->setCurrentOutputPaused(false);
    workbench->clearMessageFilters();
    if (!eventHistory->setMessageCapturePolicy(session->id, MessageCapturePolicy {})) {
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

    const QByteArray prefix = QByteArrayLiteral("{\"value\":\"");
    const QByteArray suffix = QByteArrayLiteral("\"}");
    const qsizetype bodyBytes = (std::max)(
        qsizetype(0),
        qsizetype(m_options.payloadBytes) - prefix.size() - suffix.size());
    m_payload = prefix + QByteArray(bodyBytes, 'x') + suffix;

    qInfo().noquote()
        << "PROFILE_MESSAGE_STREAM started"
        << "messages=" << m_options.messageCount
        << "batch=" << m_options.batchSize
        << "intervalMs=" << m_options.intervalMs
        << "payloadBytes=" << m_payload.size();

    samplePressure();
    m_elapsed.start();
    injectBatch();
    if (m_sentMessages < m_options.messageCount) {
        m_injectionTimer.start();
    }
}

void MessageStreamProfileDriver::injectBatch()
{
    auto *sessionService = m_viewModel.sessionService();
    auto *eventHistory = m_viewModel.eventHistory();
    const SessionState *session = sessionService ? sessionService->currentSession() : nullptr;
    if (!session || !eventHistory || m_finished) {
        return;
    }

    const int remaining = m_options.messageCount - m_sentMessages;
    const int batchCount = (std::min)(m_options.batchSize, remaining);
    for (int index = 0; index < batchCount; ++index) {
        const QString &topic = m_topics.at(m_sentMessages % m_topics.size());
        eventHistory->appendIncomingMessage(session->id, topic, m_payload);
        ++m_sentMessages;
    }
    samplePressure();

    if (m_sentMessages < m_options.messageCount) {
        return;
    }

    m_injectionTimer.stop();
    m_injectionElapsedMs = m_elapsed.elapsed();
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
    samplePressure();
    const int writerBacklog = m_lastPressure.value(QStringLiteral("backlog")).toInt();
    const int parserBacklog = m_lastPressure.value(QStringLiteral("parseBacklog")).toInt();
    if (writerBacklog == 0 && parserBacklog == 0) {
        ++m_recoveryStableSamples;
    } else {
        m_recoveryStableSamples = 0;
    }

    if (m_recoveryStableSamples >= 10) {
        finish();
    }
}

void MessageStreamProfileDriver::finish()
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    m_recoveryTimer.stop();
    samplePressure();

    auto *workbench = m_viewModel.workbench();
    QJsonObject summary {
        {QStringLiteral("sentMessages"), m_sentMessages},
        {QStringLiteral("payloadBytes"), m_payload.size()},
        {QStringLiteral("injectionElapsedMs"), m_injectionElapsedMs},
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
        {QStringLiteral("visibleMessageCount"), workbench->filteredMessages()->rowCount()},
        {QStringLiteral("finalState"),
         m_lastPressure.value(QStringLiteral("state"), QStringLiteral("normal")).toString()},
        {QStringLiteral("pressureStates"), QJsonArray::fromStringList(m_pressureStates)},
    };
    qInfo().noquote()
        << "PROFILE_MESSAGE_STREAM summary"
        << QJsonDocument(summary).toJson(QJsonDocument::Compact);
}
