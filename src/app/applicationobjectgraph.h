#pragma once

#include "viewmodels/applicationviewmodel.h"

#include <QObject>

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
    QObject m_owner;
    std::unique_ptr<ApplicationCoreState> m_state;
    ApplicationViewModel m_viewModel;
};
