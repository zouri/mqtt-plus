#pragma once

#include "appcontroller.h"

namespace ScriptStore {
struct LoadResult {
    QVector<AppController::ScriptEntry> scripts;
    bool indexWritable = true;
};

QString defaultLuaScript();
QString scriptFilePath(const QString &fileName);
QString scriptFileNameForId(const QString &id);
LoadResult loadScripts();
bool saveScripts(QVector<AppController::ScriptEntry> &scripts, bool indexWritable);
void removeScriptFile(const QString &fileName);
}
