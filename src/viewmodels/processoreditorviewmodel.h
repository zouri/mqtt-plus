#pragma once

#include "models/processorrevisionmodel.h"
#include "services/processors/processorlibrarystore.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>

class MessageProcessorEngine;
class ProcessorLibrary;

class ProcessorEditorViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentProcessorId READ currentProcessorId NOTIFY identityChanged)
    Q_PROPERTY(QString selectedRevisionId READ selectedRevisionId NOTIFY identityChanged)
    Q_PROPERTY(QString currentRevisionId READ currentRevisionId NOTIFY identityChanged)
    Q_PROPERTY(qint64 currentRevisionNumber READ currentRevisionNumber NOTIFY identityChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    Q_PROPERTY(QString languageId READ languageId NOTIFY languageChanged)
    Q_PROPERTY(QString runtimeId READ runtimeId NOTIFY languageChanged)
    Q_PROPERTY(QString runtimeName READ runtimeName NOTIFY languageChanged)
    Q_PROPERTY(QStringList languageOptionIds READ languageOptionIds CONSTANT)
    Q_PROPERTY(QStringList languageOptionNames READ languageOptionNames CONSTANT)
    Q_PROPERTY(int languageIndex READ languageIndex WRITE setLanguageIndex NOTIFY languageChanged)
    Q_PROPERTY(QString entryFile READ entryFile WRITE setEntryFile NOTIFY entryFileChanged)
    Q_PROPERTY(QString entrySymbol READ entrySymbol WRITE setEntrySymbol NOTIFY entrySymbolChanged)
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QVariantList sourceFiles READ sourceFiles NOTIFY sourceChanged)
    Q_PROPERTY(QString validationState READ validationState NOTIFY validationChanged)
    Q_PROPERTY(QString validationStatus READ validationStatus NOTIFY validationChanged)
    Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY validationChanged)
    Q_PROPERTY(bool validationOk READ validationOk NOTIFY validationChanged)
    Q_PROPERTY(bool archived READ archived NOTIFY archivedChanged)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY editorStateChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY editorStateChanged)
    Q_PROPERTY(bool canArchive READ canArchive NOTIFY editorStateChanged)
    Q_PROPERTY(bool canRestore READ canRestore NOTIFY editorStateChanged)
    Q_PROPERTY(ProcessorRevisionModel* revisions READ revisions CONSTANT)

public:
    explicit ProcessorEditorViewModel(
        ProcessorLibrary &library,
        MessageProcessorEngine &engine,
        QObject *parent = nullptr);

    QString currentProcessorId() const;
    QString selectedRevisionId() const;
    QString currentRevisionId() const;
    qint64 currentRevisionNumber() const;
    QString name() const;
    QString description() const;
    QString languageId() const;
    QString runtimeId() const;
    QString runtimeName() const;
    QStringList languageOptionIds() const;
    QStringList languageOptionNames() const;
    int languageIndex() const;
    QString entryFile() const;
    QString entrySymbol() const;
    QString source() const;
    QVariantList sourceFiles() const;
    QString validationState() const;
    QString validationStatus() const;
    QString diagnostics() const;
    bool validationOk() const;
    bool archived() const;
    bool hasUnsavedChanges() const;
    bool canSave() const;
    bool canArchive() const;
    bool canRestore() const;
    ProcessorRevisionModel *revisions();

    void setName(const QString &name);
    void setDescription(const QString &description);
    void setLanguageIndex(int index);
    void setEntryFile(const QString &entryFile);
    void setEntrySymbol(const QString &entrySymbol);
    void setSource(const QString &source);

    void newProcessor(const QString &languageId);
    bool loadProcessor(const QString &processorId);
    bool loadRevision(const QString &revisionId);
    bool validateDraft();
    SaveProcessorRevisionCommand saveCommand() const;
    void setOperationError(const QString &message);

signals:
    void identityChanged();
    void nameChanged();
    void descriptionChanged();
    void languageChanged();
    void entryFileChanged();
    void entrySymbolChanged();
    void sourceChanged();
    void validationChanged();
    void archivedChanged();
    void editorStateChanged();

private:
    struct Template
    {
        QString languageId;
        QString languageName;
        QString runtimeId;
        QString runtimeName;
        QString entryFile;
        QString entrySymbol;
        QString source;
        const char *defaultName;
        const char *defaultDescription;
        QString mediaType;
    };

    static const QVector<Template> &templates();
    static int templateIndex(const QString &languageId);
    static QString validationStateName(int state);
    void applyTemplate(int index, bool replaceMetadata);
    void loadRevisionSnapshot(const ProcessorRevisionSnapshot &revision);
    void refreshRevisions();
    void captureSavedState();
    void invalidateValidation();
    void setValidation(
        const QString &state,
        const QString &status,
        const QString &diagnostics,
        bool ok);
    void emitEditorStateChanged();

    ProcessorLibrary &m_library;
    MessageProcessorEngine &m_engine;
    ProcessorRevisionModel m_revisions;
    QString m_currentProcessorId;
    QString m_selectedRevisionId;
    QString m_currentRevisionId;
    qint64 m_currentRevisionNumber = 0;
    QString m_name;
    QString m_description;
    QString m_languageId;
    QString m_runtimeId;
    QString m_runtimeName;
    QString m_entryFile;
    QString m_entrySymbol;
    QString m_source;
    QVector<ProcessorSourceFile> m_files;
    QString m_validationState = QStringLiteral("not_validated");
    QString m_validationStatus;
    QString m_diagnostics;
    bool m_validationOk = false;
    bool m_archived = false;
    QString m_savedName;
    QString m_savedDescription;
    QString m_savedLanguageId;
    QString m_savedRuntimeId;
    QString m_savedEntryFile;
    QString m_savedEntrySymbol;
    QString m_savedSource;
};
