#pragma once

#include "domain/updaterelease.h"

#include <QObject>

class UpdateService : public QObject
{
    Q_OBJECT

public:
    enum class Error
    {
        NoReleases,
        Network,
        InvalidRelease,
    };
    Q_ENUM(Error)

    explicit UpdateService(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    virtual void fetchLatestRelease() = 0;
    virtual bool openRelease(const UpdateRelease &release) const = 0;

signals:
    void releaseFetched(const UpdateRelease &release);
    void fetchFailed(UpdateService::Error error);
};
