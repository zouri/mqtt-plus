#pragma once

#include <functional>

#include <QString>

class QObject;
class ScriptLibraryModel;

struct ScriptsCoreSignalHandlers
{
    std::function<void()> scriptLibraryChanged;
};

class ScriptsCorePort
{
public:
    virtual ~ScriptsCorePort() = default;

    virtual void bindScriptsSignals(QObject *context, const ScriptsCoreSignalHandlers &handlers) = 0;
    virtual ScriptLibraryModel *scripts() = 0;
    virtual QString upsertScript(
        const QString &id,
        const QString &name,
        const QString &description,
        const QString &code) = 0;
};
