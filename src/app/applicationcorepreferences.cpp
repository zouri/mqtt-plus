#include "app/applicationcore.h"

#include "app/applicationcorestate.h"
#include "app/applicationworkspacedependenciesfactory.h"
#include "app/settingsworkspace.h"

SettingsWorkspaceDependencies ApplicationCore::settingsWorkspaceDependencies()
{
    return m_state->workspaceDependencies.settings();
}
