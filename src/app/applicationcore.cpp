#include "app/applicationcore.h"

#include "app/applicationcorestate.h"
#include "services/apputils.h"

#include <memory>

using namespace AppUtils;

ApplicationCore::ApplicationCore(QObject *parent)
    : QObject(parent)
    , m_state(std::make_unique<ApplicationCoreState>(this))
{
    m_state->launchTimestamp = timestampNow();
    m_state->signalBindings.install(this);
    m_state->startup.run();
}

ApplicationCore::~ApplicationCore()
{
    m_state->exitCleanup.apply();
}
