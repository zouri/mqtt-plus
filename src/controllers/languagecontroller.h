#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTranslator>
#include <QVariantList>

class LanguageController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString effectiveLanguage READ effectiveLanguage NOTIFY languageChanged)
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages NOTIFY languageChanged)

public:
    explicit LanguageController(QSettings *settings, QObject *parent = nullptr);

    QString mode() const;
    QString effectiveLanguage() const;
    QVariantList availableLanguages() const;

public slots:
    void setMode(const QString &mode);

signals:
    void modeChanged();
    void languageChanged();

private:
    QString resolvedLanguage() const;
    void applyCurrentLanguage();

    QSettings *m_settings = nullptr;
    QString m_mode = QStringLiteral("system");
    QString m_effectiveLanguage = QStringLiteral("en");
    QTranslator m_translator;
    bool m_translatorInstalled = false;
};
