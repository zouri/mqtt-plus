#pragma once

#include <QByteArray>
#include <QCborMap>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct ProcessorDefinition
{
    QString id;
    QString name;
    QString description;
    QString currentRevisionId;
    QString createdAt;
    QString updatedAt;
    QString archivedAt;
};

struct ProcessorSourceFile
{
    QString path;
    QString mediaType;
    QByteArray content;
    QString contentHash;
};

struct ProcessorRevisionContent
{
    QString contractId = QStringLiteral("mqtt-plus.message-processor/v1");
    QString languageId;
    QString runtimeId;
    QString entryFile;
    QString entrySymbol = QStringLiteral("process");
    QCborMap manifest;
    QVector<ProcessorSourceFile> files;
};

struct ProcessorRevisionSnapshot
{
    QString id;
    QString processorId;
    qint64 revisionNumber = 0;
    QString contractId;
    QString languageId;
    QString runtimeId;
    QString entryFile;
    QString entrySymbol;
    QCborMap manifest;
    QString contentHash;
    QVector<ProcessorSourceFile> files;
    QString createdAt;
};

enum class ProcessorRevisionMode
{
    FollowCurrent,
    Pinned,
};

struct ProcessorReference
{
    QString processorId;
    ProcessorRevisionMode revisionMode = ProcessorRevisionMode::FollowCurrent;
    QString pinnedRevisionId;
    QCborMap parameters;
};
