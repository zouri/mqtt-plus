#pragma once

#include "services/update/updateservice.h"

#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class GitHubUpdateService final : public UpdateService
{
    Q_OBJECT

public:
    explicit GitHubUpdateService(
        const QString &applicationVersion,
        QObject *parent = nullptr);

    void fetchLatestRelease() override;
    bool openRelease(const UpdateRelease &release) const override;

private:
    void finishRequest();
    QString updateAssetNameSuffix() const;

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QString m_applicationVersion;
};
