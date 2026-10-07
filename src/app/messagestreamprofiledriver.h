#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

class ApplicationViewModel;
class QQuickItem;
class QQuickWindow;

struct MessageStreamProfileOptions
{
    int messageCount = 6000;
    int batchSize = 100;
    int intervalMs = 8;
    int payloadBytes = 2048;
    bool pauseOutput = false;
    bool varyPayload = false;
    int localBrokerPort = 0;
    int observationMs = 0;
    bool brokerInput = false;
    bool scrollOutput = false;
    bool activateWindow = false;
    bool backgroundWindow = false;
    bool captureWindow = false;
};

class MessageStreamProfileDriver : public QObject
{
    Q_OBJECT

public:
    explicit MessageStreamProfileDriver(
        ApplicationViewModel &viewModel,
        MessageStreamProfileOptions options,
        QQuickWindow &window,
        QObject *parent = nullptr);

public slots:
    void start();
    void recordRenderedFrame();

private slots:
    void injectBatch();
    void samplePressure();
    void waitForRecovery();
    void scrollOutput();

private:
    void beginObservation();
    void finish();
    void sampleWindowActivity();

    ApplicationViewModel &m_viewModel;
    QPointer<QQuickWindow> m_window;
    QPointer<QQuickItem> m_scrollList;
    const MessageStreamProfileOptions m_options;
    QTimer m_injectionTimer;
    QTimer m_recoveryTimer;
    QTimer m_scrollTimer;
    QElapsedTimer m_elapsed;
    QVector<QString> m_topics;
    QByteArray m_payload;
    qsizetype m_sequenceOffset = -1;
    QStringList m_pressureStates;
    QVariantMap m_lastPressure;
    int m_sentMessages = 0;
    int m_reportedMessageGroups = 0;
    int m_renderedFrames = 0;
    int m_recoveryStableSamples = 0;
    int m_maxWriterBacklog = 0;
    qint64 m_maxWriterBacklogBytes = 0;
    int m_maxParserBacklog = 0;
    qint64 m_maxParserBacklogBytes = 0;
    qint64 m_injectionElapsedMs = 0;
    qint64 m_firstBrokerMessageMs = -1;
    QHash<quint64, qint64> m_initialThreadCpuMicroseconds;
    qint64 m_initialProcessCpuMicroseconds = -1;
    bool m_threadCpuAvailable = false;
    bool m_started = false;
    bool m_finished = false;
    bool m_connectionRequested = false;
    bool m_brokerStayedConnected = false;
    int m_scrollEvents = 0;
    int m_manualScrollEvents = 0;
    int m_scrollReversals = 0;
    int m_scrollDirection = 1;
    qreal m_previousScrollY = 0;
    qreal m_scrollDistance = 0;
    bool m_scrollWasManual = false;
    bool m_windowInitiallyActive = false;
    int m_activeWindowSamples = 0;
    int m_inactiveWindowSamples = 0;
    int m_activeWindowSamplesAfterWarmup = 0;
    int m_inactiveWindowSamplesAfterWarmup = 0;
    bool m_windowLostActivationAfterWarmup = false;
};
