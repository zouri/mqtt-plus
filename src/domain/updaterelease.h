#pragma once

#include <QString>
#include <QUrl>

struct UpdateRelease
{
    QString version;
    QUrl releasePageUrl;
    QUrl downloadUrl;
};
