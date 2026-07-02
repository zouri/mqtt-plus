#pragma once

#include "viewmodels/applicationviewmodel.h"

#include <memory>

struct ApplicationCoreState;

class ApplicationObjectGraph
{
public:
    ApplicationObjectGraph();
    ~ApplicationObjectGraph();

    ApplicationViewModel *viewModel();
    SettingsViewModel *settingsViewModel();

private:
    std::unique_ptr<ApplicationCoreState> m_state;
    ApplicationViewModel m_viewModel;
};
