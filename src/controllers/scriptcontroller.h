#pragma once

#include "domain/script.h"

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

class ScriptController : public QObject
{
    Q_OBJECT

public:
    struct DeleteResult {
        bool success = false;
        QString fileName;
    };

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
    DeleteResult deleteScript(const QString &id);
    QVariantMap testScript(const QString &code, const QString &topic, const QString &payload, int format) const;
    void removeScriptFile(const QString &fileName) const;

signals:
    void storageError(const QString &message);

private:
    bool saveScripts();

    QVector<ScriptEntry> m_scripts;
    bool m_scriptIndexWritable = true;
};
