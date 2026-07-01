#pragma once

#include <QObject>
#include <QString>

#include "viewmodels/scripteditorviewmodel.h"

#include <functional>

class ScriptLibraryModel;

class ScriptsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ScriptLibraryModel* scripts READ scripts CONSTANT)
    Q_PROPERTY(ScriptEditorViewModel* editor READ editor CONSTANT)

public:
    struct Dependencies
    {
        ScriptLibraryModel *scripts = nullptr;
        std::function<void(QObject *, std::function<void()>)> bindScriptLibraryChanged;
        std::function<QString(const QString &, const QString &, const QString &, const QString &)> upsertScript;
    };

    explicit ScriptsViewModel(QObject *parent = nullptr);
    explicit ScriptsViewModel(const Dependencies &dependencies, QObject *parent = nullptr);

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

    Dependencies m_dependencies;
    ScriptEditorViewModel m_editor;
};
