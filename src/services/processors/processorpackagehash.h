#pragma once

#include "domain/messageprocessor.h"

#include <QString>

struct ProcessorPackageLimits
{
    int maxFiles = 64;
    qint64 maxTotalBytes = 2 * 1024 * 1024;
    qint64 maxFileBytes = 1024 * 1024;
    qint64 maxManifestBytes = 64 * 1024;
    int maxManifestDepth = 32;
};

struct PreparedProcessorPackage
{
    bool ok = false;
    ProcessorRevisionContent content;
    QByteArray manifestJson;
    QString contentHash;
    QString error;
};

namespace ProcessorPackageHash {

PreparedProcessorPackage prepare(
    const ProcessorRevisionContent &content,
    const ProcessorPackageLimits &limits = {});

} // namespace ProcessorPackageHash
