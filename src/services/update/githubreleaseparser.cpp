#include "services/update/githubreleaseparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>

namespace {

std::optional<QVersionNumber> parseVersion(QString version)
{
    version = version.trimmed();
    if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        version.remove(0, 1);
    }

    qsizetype suffixIndex = 0;
    const QVersionNumber parsed = QVersionNumber::fromString(version, &suffixIndex);
    if (parsed.isNull() || suffixIndex != version.size()) {
        return std::nullopt;
    }
    return parsed;
}

bool isHttpsUrl(const QUrl &url)
{
    return url.isValid() && url.scheme() == QStringLiteral("https") && !url.host().isEmpty();
}

} // namespace

GitHubReleaseParseResult parseGitHubLatestRelease(
    const QByteArray &payload,
    const QString &assetNameSuffix)
{
    QJsonParseError jsonError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !document.isObject()) {
        return {{}, QStringLiteral("The release response is not valid JSON.")};
    }

    const QJsonObject object = document.object();
    const QString tagName = object.value(QStringLiteral("tag_name")).toString();
    const auto versionNumber = parseVersion(tagName);
    if (!versionNumber) {
        return {{}, QStringLiteral("The release has an invalid version.")};
    }

    const QUrl releasePageUrl(object.value(QStringLiteral("html_url")).toString());
    if (!isHttpsUrl(releasePageUrl)) {
        return {{}, QStringLiteral("The release page URL is invalid.")};
    }

    UpdateRelease release;
    release.version = versionNumber->toString();
    release.releasePageUrl = releasePageUrl;

    if (!assetNameSuffix.isEmpty()) {
        const QJsonArray assets = object.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue &assetValue : assets) {
            const QJsonObject asset = assetValue.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            if (!name.endsWith(assetNameSuffix, Qt::CaseInsensitive)) {
                continue;
            }
            const QUrl downloadUrl(
                asset.value(QStringLiteral("browser_download_url")).toString());
            if (isHttpsUrl(downloadUrl)) {
                release.downloadUrl = downloadUrl;
                break;
            }
        }
    }

    return {release, {}};
}
