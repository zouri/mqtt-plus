#pragma once

#include "domain/session.h"

#include <QVariantMap>

class ApplicationSessionConfigurator
{
public:
    static void applyConfig(SessionState &session, const QVariantMap &config, bool keepNameFallback);
};
