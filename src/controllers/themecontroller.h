#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

class ThemeController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString effectiveTheme READ effectiveTheme NOTIFY effectiveThemeChanged)

public:
    explicit ThemeController(QSettings *settings, QObject *parent = nullptr);

    QString mode() const;
    QString effectiveTheme() const;

public slots:
    void setMode(const QString &mode);

signals:
    void modeChanged();
    void effectiveThemeChanged();

private:
    void refreshSystemColorScheme();

    QSettings *m_settings = nullptr;
    QString m_mode = QStringLiteral("system");
    bool m_systemDarkMode = false;
};
