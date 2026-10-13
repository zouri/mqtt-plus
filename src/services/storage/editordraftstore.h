#pragma once

#include "services/processors/processorlibrary.h"
#include <QJsonObject>
#include <QSqlDatabase>

struct EditorStoreReply
{
    int status = 200;
    QJsonObject data;
    SaveProcessorRevisionResult saved;
    QString deletedId;
};

class EditorDraftStore
{
public:
    explicit EditorDraftStore(const QString &directory);
    ~EditorDraftStore();
    EditorStoreReply request(const QString &method, const QString &path,
        const QJsonObject &body = {}, const QString &version = {}, const QString &processorId = {});
    EditorStoreReply save(const QJsonObject &draft, const QString &requestId);
    EditorStoreReply deleteProcessor(const QString &processorId);
    ProcessorLibrary &library() { return m_library; }
    bool isReady() const { return m_error.isEmpty() && m_library.isReady(); }
    QJsonObject draftCounts() const;

private:
    EditorStoreReply readDraft(const QString &id);
    EditorStoreReply writeDraft(QJsonObject draft, bool create, const QString &version);
    bool removeDraft(const QString &id, const QString &version);
    ProcessorLibrary m_library;
    QSqlDatabase m_db;
    QString m_connection;
    QString m_error;
};
