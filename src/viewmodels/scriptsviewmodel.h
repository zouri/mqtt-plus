#pragma once

#include <QObject>
#include <QString>

#include "viewmodels/scripteditorviewmodel.h"

class ScriptLibraryModel;
class ScriptsCorePort;

class ScriptsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ScriptLibraryModel* scripts READ scripts CONSTANT)
    Q_PROPERTY(ScriptEditorViewModel* editor READ editor CONSTANT)

public:
    explicit ScriptsViewModel(ScriptsCorePort *core = nullptr, QObject *parent = nullptr);

    ScriptLibraryModel *scripts() const;
    ScriptEditorViewModel *editor();
    int matchingScriptCount(const QString &filterText) const;
    static bool scriptMatchesFilter(
        const QString &name,
        const QString &description,
        const QString &code,
        const QString &filterText);

    Q_INVOKABLE void ensureEditorSelection();
    Q_INVOKABLE bool selectScriptAt(int index);
    Q_INVOKABLE void newScript();
    Q_INVOKABLE bool validateEditorStructure();
    Q_INVOKABLE bool saveEditor();
    Q_INVOKABLE int visibleScriptCount(const QString &filterText) const;

signals:
    void scriptLibraryChanged();

private:
    QString upsertScript(const QString &id, const QString &name, const QString &description, const QString &code);

    ScriptsCorePort *m_core = nullptr;
    ScriptEditorViewModel m_editor;
};
