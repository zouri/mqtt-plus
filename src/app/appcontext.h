#pragma once

#include <memory>

class AppFacade;

struct AppContext
{
    AppContext();
    ~AppContext();

    AppFacade *facade();
    const AppFacade *facade() const;

private:
    std::unique_ptr<AppFacade> m_facade;
};
