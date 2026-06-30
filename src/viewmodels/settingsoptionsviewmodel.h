#pragma once

#include <QObject>
#include <QVariant>
#include <QVariantList>

class SettingsOptionsViewModel : public QObject
{
    Q_OBJECT

public:
    explicit SettingsOptionsViewModel(QObject *parent = nullptr);

    Q_INVOKABLE int optionIndex(const QVariantList &values, const QVariant &value) const;
    Q_INVOKABLE QVariant optionValue(const QVariantList &values, int index) const;
};
