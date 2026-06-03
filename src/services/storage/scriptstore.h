#pragma once

#include "domain/script.h"

#include <QVector>

namespace ScriptStore {
struct LoadResult {
    QVector<ScriptEntry> scripts;
    bool indexWritable = true;
};

QString defaultLuaScript();
QString scriptFilePath(const QString &fileName);
QString scriptFileNameForId(const QString &id);
LoadResult loadScripts();
bool saveScripts(QVector<ScriptEntry> &scripts, bool indexWritable, QString &errorMessage);
void removeScriptFile(const QString &fileName);
}
