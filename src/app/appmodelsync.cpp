#include "app/appmodelsync.h"

#include <utility>

AppModelSync::AppModelSync(Dependencies dependencies)
    : m_coordinator(std::make_unique<ModelCoordinator>(std::move(dependencies)))
{
}

AppModelSync::~AppModelSync() = default;

void AppModelSync::refreshSessionsModel()
{
    m_coordinator->refreshSessionsModel();
}

void AppModelSync::refreshSubscriptionsModel()
{
    m_coordinator->refreshSubscriptionsModel();
}

void AppModelSync::refreshScriptsModel()
{
    m_coordinator->refreshScriptsModel();
}

void AppModelSync::refreshScriptTestSamplesModel()
{
    m_coordinator->refreshScriptTestSamplesModel();
}

void AppModelSync::syncSelectedSessionModels()
{
    m_coordinator->syncSelectedSessionModels();
}

void AppModelSync::syncSessionCollectionModels()
{
    m_coordinator->syncSessionCollectionModels();
}
