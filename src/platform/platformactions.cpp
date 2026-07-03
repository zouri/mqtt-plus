#include "platform/platformactions.h"

#include <QClipboard>
#include <QGuiApplication>

void PlatformActions::copyTextToClipboard(const QString &text) const
{
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}
