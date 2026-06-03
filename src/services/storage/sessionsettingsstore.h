#pragma once

#include "domain/session.h"

#include <QSettings>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <functional>

namespace SessionSettingsStore {

struct LoadedSession {
    SessionState session;
    QVariantMap config;
};

QVariantMap configFromState(const SessionState &session);
QVariantMap duplicateConfigFromState(const SessionState &session);
LoadedSession readSession(
    QSettings &settings,
    int index,
    const std::function<bool(const QString &)> &scriptExists);
bool writeSessions(QSettings &settings, const QVector<SessionState> &sessions, QString &errorMessage);

} // namespace SessionSettingsStore
