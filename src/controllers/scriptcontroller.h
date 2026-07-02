#pragma once

#include "domain/script.h"

#include <QObject>
#include <QString>
#include <QVector>

class ScriptController : public QObject
{
    Q_OBJECT

public:
    explicit ScriptController(QObject *parent = nullptr);

    const QVector<ScriptEntry> &scripts() const;
    const ScriptEntry *scriptById(const QString &id) const;
    QString scriptName(const QString &id) const;

    void loadScripts();
    QString upsertScript(
        const QString &id,
        const QString &name,
        const QString &description,
        const QString &code);

signals:
    void storageError(const QString &message);
    void scriptsChanged();

private:
    bool saveScripts();

    QVector<ScriptEntry> m_scripts;
    bool m_scriptIndexWritable = true;
};
