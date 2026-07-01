#pragma once

#include "app/applicationcore.h"
#include "viewmodels/applicationviewmodel.h"

class ApplicationObjectGraph
{
public:
    ApplicationObjectGraph();

    ApplicationViewModel *viewModel();
    SettingsViewModel *settingsViewModel();

private:
    ApplicationCore m_core;
    ApplicationViewModel m_viewModel;
};
