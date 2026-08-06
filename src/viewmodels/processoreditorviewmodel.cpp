#include "processoreditorviewmodel.h"

#include "domain/processorexecution.h"
#include "services/processors/messageprocessorengine.h"
#include "services/processors/processorpackagehash.h"
#include "services/processors/processorlibrary.h"

#include <QCoreApplication>

#include <algorithm>

namespace {

QString editorText(const char *source)
{
    return QCoreApplication::translate("ProcessorEditorViewModel", source);
}

QString diagnosticText(const QVector<ProcessorDiagnostic> &diagnostics)
{
    QStringList lines;
    for (const ProcessorDiagnostic &diagnostic : diagnostics) {
        QString location = diagnostic.file;
        if (!location.isEmpty() && diagnostic.line > 0) {
            location += QStringLiteral(":%1").arg(diagnostic.line);
            if (diagnostic.column > 0) {
                location += QStringLiteral(":%1").arg(diagnostic.column);
            }
        }
        QString prefix = diagnostic.code;
        if (!location.isEmpty()) {
            prefix = prefix.isEmpty()
                ? location
                : QStringLiteral("%1 · %2").arg(prefix, location);
        }
        lines.append(prefix.isEmpty()
            ? diagnostic.message
            : QStringLiteral("%1: %2").arg(prefix, diagnostic.message));
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace

ProcessorEditorViewModel::ProcessorEditorViewModel(
    ProcessorLibrary &library,
    MessageProcessorEngine &engine,
    QObject *parent)
    : QObject(parent)
    , m_library(library)
    , m_engine(engine)
    , m_validationStatus(editorText(QT_TRANSLATE_NOOP(
          "ProcessorEditorViewModel",
          "Not validated")))
{
}

QString ProcessorEditorViewModel::currentProcessorId() const { return m_currentProcessorId; }
QString ProcessorEditorViewModel::name() const { return m_name; }
QString ProcessorEditorViewModel::description() const { return m_description; }
QString ProcessorEditorViewModel::languageId() const { return m_languageId; }
QString ProcessorEditorViewModel::runtimeId() const { return m_runtimeId; }
QString ProcessorEditorViewModel::runtimeName() const { return m_runtimeName; }

QStringList ProcessorEditorViewModel::languageOptionIds() const
{
    QStringList result;
    for (const Template &entry : templates()) {
        result.append(entry.languageId);
    }
    return result;
}

QStringList ProcessorEditorViewModel::languageOptionNames() const
{
    QStringList result;
    for (const Template &entry : templates()) {
        result.append(entry.languageName);
    }
    return result;
}

int ProcessorEditorViewModel::languageIndex() const
{
    return std::max(0, templateIndex(m_languageId));
}

QString ProcessorEditorViewModel::entryFile() const { return m_entryFile; }
QString ProcessorEditorViewModel::entrySymbol() const { return m_entrySymbol; }
QString ProcessorEditorViewModel::source() const { return m_source; }

QVariantList ProcessorEditorViewModel::sourceFiles() const
{
    QVariantList result;
    result.reserve(m_files.size());
    for (const ProcessorSourceFile &file : m_files) {
        result.append(QVariantMap {
            {QStringLiteral("path"), file.path},
            {QStringLiteral("mediaType"), file.mediaType},
            {QStringLiteral("content"), QString::fromUtf8(file.content)},
        });
    }
    return result;
}
QString ProcessorEditorViewModel::validationState() const { return m_validationState; }
QString ProcessorEditorViewModel::validationStatus() const { return m_validationStatus; }
QString ProcessorEditorViewModel::diagnostics() const { return m_diagnostics; }
bool ProcessorEditorViewModel::validationOk() const { return m_validationOk; }
bool ProcessorEditorViewModel::hasUnsavedChanges() const
{
    return m_name != m_savedName
        || m_description != m_savedDescription
        || m_languageId != m_savedLanguageId
        || m_runtimeId != m_savedRuntimeId
        || m_entryFile != m_savedEntryFile
        || m_entrySymbol != m_savedEntrySymbol
        || m_source != m_savedSource;
}

bool ProcessorEditorViewModel::canSave() const
{
    return !m_name.trimmed().isEmpty()
        && (m_currentProcessorId.isEmpty() || hasUnsavedChanges());
}

void ProcessorEditorViewModel::setName(const QString &name)
{
    if (m_name == name) {
        return;
    }
    m_name = name;
    emit nameChanged();
    invalidateValidation();
}

void ProcessorEditorViewModel::setDescription(const QString &description)
{
    if (m_description == description) {
        return;
    }
    m_description = description;
    emit descriptionChanged();
    emitEditorStateChanged();
}

void ProcessorEditorViewModel::setLanguageIndex(int index)
{
    if (index < 0 || index >= templates().size()
        || templates().at(index).languageId == m_languageId) {
        return;
    }
    applyTemplate(index, false);
    invalidateValidation();
}

void ProcessorEditorViewModel::setEntryFile(const QString &entryFile)
{
    if (m_entryFile == entryFile) {
        return;
    }
    const QString previousEntryFile = m_entryFile;
    m_entryFile = entryFile;
    for (ProcessorSourceFile &file : m_files) {
        if (file.path == previousEntryFile) {
            file.path = entryFile;
            file.contentHash.clear();
            break;
        }
    }
    emit entryFileChanged();
    invalidateValidation();
}

void ProcessorEditorViewModel::setEntrySymbol(const QString &entrySymbol)
{
    if (m_entrySymbol == entrySymbol) {
        return;
    }
    m_entrySymbol = entrySymbol;
    emit entrySymbolChanged();
    invalidateValidation();
}

void ProcessorEditorViewModel::setSource(const QString &source)
{
    if (m_source == source) {
        return;
    }
    m_source = source;
    for (ProcessorSourceFile &file : m_files) {
        if (file.path == m_entryFile) {
            file.content = source.toUtf8();
            file.contentHash.clear();
            break;
        }
    }
    emit sourceChanged();
    invalidateValidation();
}

void ProcessorEditorViewModel::newProcessor(const QString &languageId)
{
    m_currentProcessorId.clear();
    m_currentRevisionNumber = 0;
    applyTemplate(std::max(0, templateIndex(languageId)), true);
    captureSavedState();
    setValidation(
        QStringLiteral("not_validated"),
        editorText(QT_TRANSLATE_NOOP("ProcessorEditorViewModel", "Not validated")),
        {},
        false);
    emit identityChanged();
    emitEditorStateChanged();
}

bool ProcessorEditorViewModel::loadProcessor(const QString &processorId)
{
    const auto processor = m_library.processorById(processorId);
    if (!processor) {
        setOperationError(editorText(QT_TRANSLATE_NOOP(
            "ProcessorEditorViewModel",
            "Processor is unavailable.")));
        return false;
    }
    m_currentProcessorId = processor->id;
    m_name = processor->name;
    m_description = processor->description;
    const auto revision = m_library.revisionById(processor->currentRevisionId);
    if (revision.isNull()) {
        setOperationError(editorText(QT_TRANSLATE_NOOP(
            "ProcessorEditorViewModel",
            "Message Processor content is unavailable.")));
        emit identityChanged();
        emit nameChanged();
        emit descriptionChanged();
        emitEditorStateChanged();
        return false;
    }
    loadRevisionSnapshot(*revision);
    captureSavedState();
    validateDraft();
    emit identityChanged();
    emit nameChanged();
    emit descriptionChanged();
    emitEditorStateChanged();
    return true;
}

bool ProcessorEditorViewModel::validateDraft()
{
    if (m_name.trimmed().isEmpty()) {
        setValidation(
            QStringLiteral("invalid_source"),
            editorText(QT_TRANSLATE_NOOP(
                "ProcessorEditorViewModel",
                "Processor name is required.")),
            {},
            false);
        return false;
    }

    const SaveProcessorRevisionCommand command = saveCommand();
    const PreparedProcessorPackage package = ProcessorPackageHash::prepare(command.content);
    if (!package.ok) {
        setValidation(
            QStringLiteral("invalid_source"),
            editorText(QT_TRANSLATE_NOOP("ProcessorEditorViewModel", "Validation failed")),
            package.error,
            false);
        return false;
    }

    ProcessorRevisionSnapshot preview;
    preview.id = QStringLiteral("editor-preview");
    preview.processorId = m_currentProcessorId.isEmpty()
        ? QStringLiteral("new-processor")
        : m_currentProcessorId;
    preview.revisionNumber = std::max<qint64>(1, m_currentRevisionNumber + 1);
    preview.contractId = package.content.contractId;
    preview.languageId = package.content.languageId;
    preview.runtimeId = package.content.runtimeId;
    preview.entryFile = package.content.entryFile;
    preview.entrySymbol = package.content.entrySymbol;
    preview.manifest = package.content.manifest;
    preview.contentHash = package.contentHash;
    preview.files = package.content.files;
    const ProcessorValidationResult validation = m_engine.validate(preview);
    const bool ready = validation.isReady();
    setValidation(
        validationStateName(static_cast<int>(validation.state)),
        ready
            ? editorText(QT_TRANSLATE_NOOP("ProcessorEditorViewModel", "Ready"))
            : editorText(QT_TRANSLATE_NOOP("ProcessorEditorViewModel", "Validation failed")),
        diagnosticText(validation.diagnostics),
        ready);
    return ready;
}

SaveProcessorRevisionCommand ProcessorEditorViewModel::saveCommand() const
{
    SaveProcessorRevisionCommand command;
    command.processorId = m_currentProcessorId;
    command.name = m_name;
    command.description = m_description;
    command.content.languageId = m_languageId;
    command.content.runtimeId = m_runtimeId;
    command.content.entryFile = m_entryFile;
    command.content.entrySymbol = m_entrySymbol;
    command.content.files = m_files;
    bool entryFileFound = false;
    for (ProcessorSourceFile &file : command.content.files) {
        if (file.path == m_entryFile) {
            file.content = m_source.toUtf8();
            file.contentHash.clear();
            entryFileFound = true;
            break;
        }
    }
    if (!entryFileFound) {
        command.content.files.append({
            m_entryFile,
            templates().at(std::max(0, templateIndex(m_languageId))).mediaType,
            m_source.toUtf8(),
            {},
        });
    }
    return command;
}

void ProcessorEditorViewModel::setOperationError(const QString &message)
{
    setValidation(
        QStringLiteral("operation_failed"),
        editorText(QT_TRANSLATE_NOOP("ProcessorEditorViewModel", "Operation failed")),
        message,
        false);
}

const QVector<ProcessorEditorViewModel::Template> &ProcessorEditorViewModel::templates()
{
    static const QVector<Template> entries {
        {
            QStringLiteral("lua"),
            QStringLiteral("Lua"),
            QStringLiteral("lua-5.5"),
            QStringLiteral("Lua 5.5"),
            QStringLiteral("main.lua"),
            QStringLiteral("process"),
            QStringLiteral(
                "function process(context)\n"
                "    return context.decoded\n"
                "end\n"),
            QT_TRANSLATE_NOOP(
                "ProcessorEditorViewModel",
                "New Lua Processor"),
            QT_TRANSLATE_NOOP(
                "ProcessorEditorViewModel",
                "Transform MQTT messages with Lua."),
            QStringLiteral("text/x-lua"),
        },
        {
            QStringLiteral("javascript"),
            QStringLiteral("JavaScript"),
            QStringLiteral("qt-qjs"),
            QStringLiteral("JavaScript (Qt QJSEngine)"),
            QStringLiteral("main.js"),
            QStringLiteral("process"),
            QStringLiteral(
                "function process(context) {\n"
                "    return context.decoded;\n"
                "}\n"),
            QT_TRANSLATE_NOOP(
                "ProcessorEditorViewModel",
                "New JavaScript Processor"),
            QT_TRANSLATE_NOOP(
                "ProcessorEditorViewModel",
                "Transform MQTT messages with JavaScript."),
            QStringLiteral("text/javascript"),
        },
    };
    return entries;
}

int ProcessorEditorViewModel::templateIndex(const QString &languageId)
{
    for (qsizetype index = 0; index < templates().size(); ++index) {
        if (templates().at(index).languageId == languageId) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

QString ProcessorEditorViewModel::validationStateName(int state)
{
    switch (static_cast<ProcessorValidationState>(state)) {
    case ProcessorValidationState::Ready:
        return QStringLiteral("ready");
    case ProcessorValidationState::InvalidSource:
        return QStringLiteral("invalid_source");
    case ProcessorValidationState::RuntimeUnavailable:
        return QStringLiteral("runtime_unavailable");
    case ProcessorValidationState::PreparationFailed:
        return QStringLiteral("preparation_failed");
    case ProcessorValidationState::InternalError:
        return QStringLiteral("internal_error");
    }
    return QStringLiteral("internal_error");
}

void ProcessorEditorViewModel::applyTemplate(int index, bool replaceMetadata)
{
    const Template &entry = templates().at(index);
    const bool languageIdentityChanged = m_languageId != entry.languageId
        || m_runtimeId != entry.runtimeId
        || m_runtimeName != entry.runtimeName;
    m_languageId = entry.languageId;
    m_runtimeId = entry.runtimeId;
    m_runtimeName = entry.runtimeName;
    m_entryFile = entry.entryFile;
    m_entrySymbol = entry.entrySymbol;
    m_source = entry.source;
    m_files = {
        {
            entry.entryFile,
            entry.mediaType,
            entry.source.toUtf8(),
            {},
        },
    };
    if (replaceMetadata) {
        m_name = editorText(entry.defaultName);
        m_description = editorText(entry.defaultDescription);
        emit nameChanged();
        emit descriptionChanged();
    }
    if (languageIdentityChanged) {
        emit languageChanged();
    }
    emit entryFileChanged();
    emit entrySymbolChanged();
    emit sourceChanged();
    emitEditorStateChanged();
}

void ProcessorEditorViewModel::loadRevisionSnapshot(
    const ProcessorRevisionSnapshot &revision)
{
    m_currentRevisionNumber = revision.revisionNumber;
    m_languageId = revision.languageId;
    m_runtimeId = revision.runtimeId;
    const int index = templateIndex(revision.languageId);
    m_runtimeName = index >= 0 ? templates().at(index).runtimeName : revision.runtimeId;
    m_entryFile = revision.entryFile;
    m_entrySymbol = revision.entrySymbol;
    m_files = revision.files;
    m_source.clear();
    for (const ProcessorSourceFile &file : revision.files) {
        if (file.path == revision.entryFile) {
            m_source = QString::fromUtf8(file.content);
            break;
        }
    }
    emit languageChanged();
    emit entryFileChanged();
    emit entrySymbolChanged();
    emit sourceChanged();
}

void ProcessorEditorViewModel::captureSavedState()
{
    m_savedName = m_name;
    m_savedDescription = m_description;
    m_savedLanguageId = m_languageId;
    m_savedRuntimeId = m_runtimeId;
    m_savedEntryFile = m_entryFile;
    m_savedEntrySymbol = m_entrySymbol;
    m_savedSource = m_source;
}

void ProcessorEditorViewModel::invalidateValidation()
{
    setValidation(
        QStringLiteral("not_validated"),
        editorText(QT_TRANSLATE_NOOP("ProcessorEditorViewModel", "Not validated")),
        {},
        false);
    emitEditorStateChanged();
}

void ProcessorEditorViewModel::setValidation(
    const QString &state,
    const QString &status,
    const QString &diagnostics,
    bool ok)
{
    if (m_validationState == state
        && m_validationStatus == status
        && m_diagnostics == diagnostics
        && m_validationOk == ok) {
        return;
    }
    m_validationState = state;
    m_validationStatus = status;
    m_diagnostics = diagnostics;
    m_validationOk = ok;
    emit validationChanged();
}

void ProcessorEditorViewModel::emitEditorStateChanged()
{
    emit editorStateChanged();
}
