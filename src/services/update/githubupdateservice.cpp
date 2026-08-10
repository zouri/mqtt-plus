#include "services/update/githubupdateservice.h"

#include "services/update/githubreleaseparser.h"

#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>

namespace {

constexpr auto kLatestReleaseUrl = "https://api.github.com/repos/zouri/mqtt-plus/releases/latest";

} // namespace

GitHubUpdateService::GitHubUpdateService(
    const QString &applicationVersion,
    QObject *parent)
    : UpdateService(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_applicationVersion(applicationVersion)
{
}

void GitHubUpdateService::fetchLatestRelease()
{
    if (m_reply) {
        return;
    }

    QNetworkRequest request {QUrl(QString::fromLatin1(kLatestReleaseUrl))};
    request.setRawHeader(
        QByteArrayLiteral("Accept"),
        QByteArrayLiteral("application/vnd.github+json"));
    request.setRawHeader(
        QByteArrayLiteral("User-Agent"),
        QByteArrayLiteral("mqtt-plus/") + m_applicationVersion.toUtf8());
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &GitHubUpdateService::finishRequest);
}

bool GitHubUpdateService::openRelease(const UpdateRelease &release) const
{
    const QUrl target = release.downloadUrl.isValid()
        ? release.downloadUrl
        : release.releasePageUrl;
    return target.isValid() && QDesktopServices::openUrl(target);
}

void GitHubUpdateService::finishRequest()
{
    QNetworkReply *reply = m_reply.data();
    m_reply = nullptr;
    if (!reply) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const int statusCode = reply
                                   ->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                                   .toInt();
        reply->deleteLater();
        emit fetchFailed(
            statusCode == 404 ? Error::NoReleases : Error::Network);
        return;
    }

    const QByteArray payload = reply->readAll();
    reply->deleteLater();
    const GitHubReleaseParseResult result = parseGitHubLatestRelease(
        payload,
        updateAssetNameSuffix());
    if (!result.release) {
        emit fetchFailed(Error::InvalidRelease);
        return;
    }

    emit releaseFetched(*result.release);
}

QString GitHubUpdateService::updateAssetNameSuffix() const
{
#if defined(Q_OS_MACOS)
    QString architecture = QSysInfo::currentCpuArchitecture().toLower();
    if (architecture == QStringLiteral("aarch64")) {
        architecture = QStringLiteral("arm64");
    } else if (architecture == QStringLiteral("amd64")) {
        architecture = QStringLiteral("x86_64");
    }
    if (architecture == QStringLiteral("arm64")
        || architecture == QStringLiteral("x86_64")) {
        return QStringLiteral("-macos-%1.dmg").arg(architecture);
    }
#endif
    return {};
}
