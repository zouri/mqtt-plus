#pragma once

#include <QObject>
#include <QString>

class NavigationViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentView READ currentView WRITE setCurrentView NOTIFY currentViewChanged)

public:
    explicit NavigationViewModel(QObject *parent = nullptr);

    QString currentView() const;
    void setCurrentView(const QString &view);

signals:
    void currentViewChanged();

private:
    QString m_currentView = QStringLiteral("workbench");
};
