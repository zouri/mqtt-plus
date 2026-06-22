#include "app/appfacade.h"

void AppFacade::appendEvent(SessionState &session, const QString &channel, const QString &message)
{
    m_eventController.appendEvent(session, channel, message);
}

void AppFacade::reloadCurrentSessionHistory()
{
    m_eventController.reloadCurrentSessionHistory();
}
