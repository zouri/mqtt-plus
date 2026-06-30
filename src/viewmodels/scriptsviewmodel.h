#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "viewmodels/scripteditorviewmodel.h"

class ApplicationCore;
class ScriptLibraryModel;
class ScriptTestSamplesModel;

class ScriptsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ScriptLibraryModel* scripts READ scripts CONSTANT)
    Q_PROPERTY(ScriptTestSamplesModel* scriptTestSamples READ scriptTestSamples CONSTANT)
    Q_PROPERTY(ScriptEditorViewModel* editor READ editor CONSTANT)
    Q_PROPERTY(QStringList payloadFormats READ payloadFormats CONSTANT)

public:
    explicit ScriptsViewModel(ApplicationCore *core = nullptr, QObject *parent = nullptr);

    ScriptLibraryModel *scripts() const;
    ScriptTestSamplesModel *scriptTestSamples() const;
    ScriptEditorViewModel *editor();
    QStringList payloadFormats() const;
    int matchingScriptCount(const QString &filterText) const;
    static bool scriptMatchesFilter(
        const QString &name,
        const QString &description,
        const QString &code,
        const QString &filterText);

    Q_INVOKABLE void ensureEditorSelection();
    Q_INVOKABLE bool selectScriptAt(int index);
    Q_INVOKABLE bool saveEditor();
    Q_INVOKABLE int visibleScriptCount(const QString &filterText) const;
    Q_INVOKABLE QString upsertScript(const QString &id, const QString &name, const QString &description, const QString &code);
    Q_INVOKABLE bool deleteScript(const QString &id);
    Q_INVOKABLE QVariantMap testScript(const QString &code, const QString &topic, const QString &payload, int format) const;

signals:
    void scriptLibraryChanged();
    void scriptTestSamplesChanged();

private:
    ApplicationCore *m_core = nullptr;
    ScriptEditorViewModel m_editor;
};
