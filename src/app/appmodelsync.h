#pragma once

#include "app/modelcoordinator.h"

#include <memory>

class AppModelSync
{
public:
    using Dependencies = ModelCoordinator::Dependencies;

    explicit AppModelSync(Dependencies dependencies);
    ~AppModelSync();

    void refreshSessionsModel();
    void refreshSubscriptionsModel();
    void refreshScriptsModel();
    void refreshScriptTestSamplesModel();
    void syncSelectedSessionModels();
    void syncSessionCollectionModels();

private:
    std::unique_ptr<ModelCoordinator> m_coordinator;
};
