#pragma once

#include "domain/updaterelease.h"

#include <QByteArray>
#include <QString>

#include <optional>

struct GitHubReleaseParseResult
{
    std::optional<UpdateRelease> release;
    QString error;
};

GitHubReleaseParseResult parseGitHubLatestRelease(
    const QByteArray &payload,
    const QString &assetNameSuffix);
