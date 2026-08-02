#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

class ApplicationViewModel;

struct MessageStreamProfileOptions
{
    int messageCount = 6000;
    int batchSize = 100;
    int intervalMs = 8;
    int payloadBytes = 2048;
};

class MessageStreamProfileDriver : public QObject
{
    Q_OBJECT

public:
    explicit MessageStreamProfileDriver(
        ApplicationViewModel &viewModel,
        MessageStreamProfileOptions options,
        QObject *parent = nullptr);

public slots:
    void start();

private slots:
    void injectBatch();
    void samplePressure();
    void waitForRecovery();

private:
    void finish();

    ApplicationViewModel &m_viewModel;
    const MessageStreamProfileOptions m_options;
    QTimer m_injectionTimer;
    QTimer m_recoveryTimer;
    QElapsedTimer m_elapsed;
    QVector<QString> m_topics;
    QByteArray m_payload;
    QStringList m_pressureStates;
    QVariantMap m_lastPressure;
    int m_sentMessages = 0;
    int m_recoveryStableSamples = 0;
    int m_maxWriterBacklog = 0;
    qint64 m_maxWriterBacklogBytes = 0;
    int m_maxParserBacklog = 0;
    qint64 m_maxParserBacklogBytes = 0;
    qint64 m_injectionElapsedMs = 0;
    bool m_started = false;
    bool m_finished = false;
};
