#include "viewmodels/navigationviewmodel.h"

NavigationViewModel::NavigationViewModel(QObject *parent)
    : QObject(parent)
{
}

QString NavigationViewModel::currentView() const
{
    return m_currentView;
}

void NavigationViewModel::setCurrentView(const QString &view)
{
    const QString normalized = view == QStringLiteral("logs")
            || view == QStringLiteral("scripts")
            || view == QStringLiteral("settings")
        ? view
        : QStringLiteral("workbench");
    if (m_currentView == normalized) {
        return;
    }

    m_currentView = normalized;
    emit currentViewChanged();
}
