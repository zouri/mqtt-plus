#include "languagecontroller.h"

#include <QCoreApplication>
#include <QLocale>
#include <QVariantMap>

namespace {
QString sanitizeLanguageMode(const QString &value)
{
    const QString mode = value.trimmed();
    if (mode == QStringLiteral("en") || mode == QStringLiteral("zh_CN")) {
        return mode;
    }
    return QStringLiteral("system");
}
}

LanguageController::LanguageController(QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    if (m_settings) {
        m_mode = sanitizeLanguageMode(
            m_settings->value(QStringLiteral("appearance/languageMode"), QStringLiteral("system")).toString());
    }
    applyCurrentLanguage();
}

QString LanguageController::mode() const
{
    return m_mode;
}

QString LanguageController::effectiveLanguage() const
{
    return m_effectiveLanguage;
}

QVariantList LanguageController::availableLanguages() const
{
    QVariantList languages;

    QVariantMap system;
    system.insert(QStringLiteral("mode"), QStringLiteral("system"));
    system.insert(QStringLiteral("label"), tr("System"));
    languages.append(system);

    QVariantMap english;
    english.insert(QStringLiteral("mode"), QStringLiteral("en"));
    english.insert(QStringLiteral("label"), QStringLiteral("English"));
    languages.append(english);

    QVariantMap simplifiedChinese;
    simplifiedChinese.insert(QStringLiteral("mode"), QStringLiteral("zh_CN"));
    simplifiedChinese.insert(QStringLiteral("label"), QStringLiteral("简体中文"));
    languages.append(simplifiedChinese);

    return languages;
}

void LanguageController::setMode(const QString &mode)
{
    const QString sanitized = sanitizeLanguageMode(mode);
    if (sanitized == m_mode) {
        return;
    }

    m_mode = sanitized;
    if (m_settings) {
        m_settings->setValue(QStringLiteral("appearance/languageMode"), m_mode);
        m_settings->sync();
    }

    applyCurrentLanguage();
    emit modeChanged();
    emit languageChanged();
}

QString LanguageController::resolvedLanguage() const
{
    if (m_mode == QStringLiteral("en") || m_mode == QStringLiteral("zh_CN")) {
        return m_mode;
    }

    const QLocale systemLocale;
    return systemLocale.language() == QLocale::Chinese ? QStringLiteral("zh_CN") : QStringLiteral("en");
}

void LanguageController::applyCurrentLanguage()
{
    if (m_translatorInstalled) {
        QCoreApplication::removeTranslator(&m_translator);
        m_translatorInstalled = false;
    }

    m_effectiveLanguage = resolvedLanguage();
    if (m_effectiveLanguage != QStringLiteral("zh_CN")) {
        return;
    }

    if (m_translator.load(QStringLiteral(":/i18n/mqtt_plus_zh_CN.qm"))) {
        QCoreApplication::installTranslator(&m_translator);
        m_translatorInstalled = true;
    }
}
