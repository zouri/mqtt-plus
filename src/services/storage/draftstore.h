#pragma once

#include "domain/publishdraft.h"

#include <QString>
#include <QVector>

namespace DraftStore {

enum class LoadState {
    Ready,
    Corrupt,
    Incompatible,
};

struct LoadResult {
    QVector<PublishDraft> drafts;
    LoadState state = LoadState::Ready;
    QString errorMessage;
    bool canRecover = false;
};

struct SaveResult {
    bool ok = false;
    QString errorMessage;
};

QString storageDirectory(const QString &overrideRoot = QString());
QString primaryFilePath(const QString &overrideRoot = QString());
QString backupFilePath(const QString &overrideRoot = QString());
LoadResult loadDrafts(const QString &overrideRoot = QString());
SaveResult saveDrafts(const QVector<PublishDraft> &drafts, const QString &overrideRoot = QString());
SaveResult recoverBackup(const QString &overrideRoot = QString());

} // namespace DraftStore
