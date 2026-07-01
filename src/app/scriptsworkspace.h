#pragma once

#include "viewmodels/scriptscoreport.h"

#include <QObject>

#include <functional>

class ScriptController;
class ScriptLibraryModel;

struct ScriptsWorkspaceDependencies {
    ScriptLibraryModel *scripts = nullptr;
    ScriptController *scriptController = nullptr;
    std::function<void(QObject *, std::function<void()>)> bindScriptLibraryChanged;
    std::function<void()> refreshScriptsModel;
    std::function<void()> emitScriptLibraryChanged;
    std::function<void()> notifyCurrentSessionAndSubscriptionsChanged;
};

class ScriptsWorkspace : public ScriptsCorePort
{
public:
    explicit ScriptsWorkspace(const ScriptsWorkspaceDependencies &dependencies = {});

    void bindScriptsSignals(QObject *context, const ScriptsCoreSignalHandlers &handlers) override;
    ScriptLibraryModel *scripts() override;
    QString upsertScript(
        const QString &id,
        const QString &name,
        const QString &description,
        const QString &code) override;

private:
    ScriptsWorkspaceDependencies m_dependencies;
};
