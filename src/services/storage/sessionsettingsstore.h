#pragma once

#include "domain/session.h"

#include <QSettings>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace SessionSettingsStore {

struct LoadedSession {
    SessionState session;
    QVariantMap config;
};

QVariantMap configFromState(const SessionState &session);
QVariantMap duplicateConfigFromState(const SessionState &session);
LoadedSession readSession(QSettings &settings, int index);
bool writeSessions(QSettings &settings, const QVector<SessionState> &sessions, QString &errorMessage);

} // namespace SessionSettingsStore
