#include "viewmodels/settingsoptionsviewmodel.h"

#include <algorithm>

SettingsOptionsViewModel::SettingsOptionsViewModel(QObject *parent)
    : QObject(parent)
{
}

int SettingsOptionsViewModel::optionIndex(const QVariantList &values, const QVariant &value) const
{
    for (int i = 0; i < values.size(); ++i) {
        if (values.at(i) == value) {
            return i;
        }
    }
    return 0;
}

QVariant SettingsOptionsViewModel::optionValue(const QVariantList &values, int index) const
{
    if (values.isEmpty()) {
        return {};
    }
    return values.at(std::clamp(index, 0, static_cast<int>(values.size()) - 1));
}
