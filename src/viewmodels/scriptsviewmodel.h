#pragma once

#include <QObject>
#include <QString>

#include "models/scriptfiltermodel.h"
#include "viewmodels/scripteditorviewmodel.h"

class ScriptLibraryModel;
class ScriptService;

class ScriptsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ScriptLibraryModel* scripts READ scripts CONSTANT)
    Q_PROPERTY(ScriptFilterModel* filteredScripts READ filteredScripts CONSTANT)
    Q_PROPERTY(ScriptEditorViewModel* editor READ editor CONSTANT)

public:
    explicit ScriptsViewModel(
        ScriptService &scriptService,
        ScriptLibraryModel &scripts,
        QObject *parent = nullptr);

    ScriptLibraryModel *scripts() const;
    ScriptFilterModel *filteredScripts();
    ScriptEditorViewModel *editor();

    Q_INVOKABLE void ensureEditorSelection();
    Q_INVOKABLE bool selectScriptAt(int index);
    Q_INVOKABLE bool selectFilteredScriptAt(int index);
    Q_INVOKABLE void setScriptFilterText(const QString &filterText);
    Q_INVOKABLE void newScript();
    Q_INVOKABLE bool validateEditorStructure();
    Q_INVOKABLE bool saveEditor();

signals:
    void scriptLibraryChanged();

private:
    ScriptService &m_scriptService;
    ScriptLibraryModel &m_scripts;
    ScriptFilterModel m_filteredScripts;
    ScriptEditorViewModel m_editor;
};
