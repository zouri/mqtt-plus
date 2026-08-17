#include "viewmodels/updateviewmodel.h"

#include "usecases/updatecontroller.h"

#include <QCoreApplication>

namespace {

QString updateText(const char *source)
{
    return QCoreApplication::translate("UpdateViewModel", source);
}

} // namespace

UpdateViewModel::UpdateViewModel(
    UpdateController &controller,
    QObject *parent)
    : QObject(parent)
    , m_controller(controller)
{
    connect(
        &m_controller,
        &UpdateController::stateChanged,
        this,
        [this]() {
            refreshStatusMessage();
            emit updateStateChanged();
        });
    connect(
        &m_controller,
        &UpdateController::automaticChecksEnabledChanged,
        this,
        &UpdateViewModel::automaticChecksEnabledChanged);
    refreshStatusMessage();
}

QString UpdateViewModel::currentVersion() const
{
    return m_controller.currentVersion();
}

QString UpdateViewModel::latestVersion() const
{
    return m_controller.latestVersion();
}

QString UpdateViewModel::statusMessage() const
{
    return m_statusMessage;
}

bool UpdateViewModel::busy() const
{
    return m_controller.busy();
}

bool UpdateViewModel::updateAvailable() const
{
    return m_controller.updateAvailable();
}

bool UpdateViewModel::directDownloadAvailable() const
{
    return m_controller.directDownloadAvailable();
}

bool UpdateViewModel::automaticChecksEnabled() const
{
    return m_controller.automaticChecksEnabled();
}

void UpdateViewModel::setAutomaticChecksEnabled(bool enabled)
{
    m_controller.setAutomaticChecksEnabled(enabled);
}

void UpdateViewModel::checkForUpdates()
{
    m_controller.checkForUpdates(UpdateController::CheckMode::Manual);
}

bool UpdateViewModel::openDownloadPage() const
{
    return m_controller.openDownloadPage();
}

void UpdateViewModel::retranslate()
{
    refreshStatusMessage();
    emit updateStateChanged();
}

void UpdateViewModel::refreshStatusMessage()
{
    switch (m_controller.status()) {
    case UpdateController::Status::Idle:
        m_statusMessage = updateText(QT_TRANSLATE_NOOP(
            "UpdateViewModel",
            "Updates are provided through GitHub Releases."));
        break;
    case UpdateController::Status::Checking:
        m_statusMessage = updateText(QT_TRANSLATE_NOOP(
            "UpdateViewModel",
            "Checking GitHub Releases..."));
        break;
    case UpdateController::Status::UpdateAvailable:
        m_statusMessage = updateText(QT_TRANSLATE_NOOP(
            "UpdateViewModel",
            "Version %1 is available."))
                              .arg(m_controller.latestVersion());
        break;
    case UpdateController::Status::UpToDate:
        m_statusMessage = updateText(QT_TRANSLATE_NOOP(
            "UpdateViewModel",
            "You are using the latest version."));
        break;
    case UpdateController::Status::NoReleases:
        m_statusMessage = updateText(QT_TRANSLATE_NOOP(
            "UpdateViewModel",
            "No published releases were found."));
        break;
    case UpdateController::Status::NetworkError:
        m_statusMessage = updateText(QT_TRANSLATE_NOOP(
            "UpdateViewModel",
            "Could not check for updates. Check your network connection and try again."));
        break;
    case UpdateController::Status::InvalidRelease:
        m_statusMessage = updateText(QT_TRANSLATE_NOOP(
            "UpdateViewModel",
            "GitHub returned release information that this version cannot read."));
        break;
    }
}
