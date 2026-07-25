#pragma once

#include <QObject>
#include <QString>

#include "viewmodels/scripteditorviewmodel.h"

class ScriptLibraryModel;
class ScriptService;

class ScriptsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ScriptLibraryModel* scripts READ scripts CONSTANT)
    Q_PROPERTY(ScriptEditorViewModel* editor READ editor CONSTANT)

public:
    explicit ScriptsViewModel(
        ScriptService &scriptService,
        ScriptLibraryModel &scripts,
        QObject *parent = nullptr);

    ScriptLibraryModel *scripts() const;
    ScriptEditorViewModel *editor();
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
    ScriptService &m_scriptService;
    ScriptLibraryModel &m_scripts;
    ScriptEditorViewModel m_editor;
};
