#pragma once

#include "domain/session.h"
#include "domain/sessionconfig.h"

#include <QSettings>
#include <QString>
#include <QVector>

namespace SessionSettingsStore {

struct LoadedSession {
    SessionState session;
    SessionConnectionConfig config;
};

SessionConnectionConfig configFromState(const SessionState &session);
SessionConnectionConfig duplicateConfigFromState(const SessionState &session);
LoadedSession readSession(QSettings &settings, int index);
bool writeSessions(QSettings &settings, const QVector<SessionState> &sessions, QString &errorMessage);

} // namespace SessionSettingsStore
