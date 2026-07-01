#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class ScriptEditorViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentScriptId READ currentScriptId WRITE setCurrentScriptId NOTIFY currentScriptIdChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    Q_PROPERTY(QString code READ code WRITE setCode NOTIFY codeChanged)
    Q_PROPERTY(QString validationStatus READ validationStatus NOTIFY validationStatusChanged)
    Q_PROPERTY(bool validationOk READ validationOk NOTIFY validationOkChanged)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY editorStateChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY editorStateChanged)

public:
    explicit ScriptEditorViewModel(QObject *parent = nullptr);

    QString currentScriptId() const;
    QString name() const;
    QString description() const;
    QString code() const;
    QString validationStatus() const;
    bool validationOk() const;
    bool hasUnsavedChanges() const;
    bool canSave() const;

    void setCurrentScriptId(const QString &id);
    void setName(const QString &name);
    void setDescription(const QString &description);
    void setCode(const QString &code);

    QString defaultCode() const;
    void loadScript(const QVariantMap &row);
    void newScript();
    bool validateStructure();
    void markSaved(const QString &id);

signals:
    void currentScriptIdChanged();
    void nameChanged();
    void descriptionChanged();
    void codeChanged();
    void validationStatusChanged();
    void validationOkChanged();
    void editorStateChanged();

private:
    void setValidationStatus(const QString &status);
    void setValidationOk(bool ok);
    void emitEditorStateChanged();

    QString m_currentScriptId;
    QString m_name;
    QString m_description;
    QString m_code;
    QString m_savedName;
    QString m_savedDescription;
    QString m_savedCode;
    QString m_validationStatus = QStringLiteral("Unsaved");
    bool m_validationOk = false;
};
