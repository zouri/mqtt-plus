#include "processorlibrarymodel.h"

#include "domain/processorexecution.h"
#include "services/apputils.h"
#include "services/processors/messageprocessorengine.h"
#include "services/processors/processorlibrary.h"

#include <utility>

using namespace AppUtils;

namespace {

QString validationStateName(ProcessorValidationState state)
{
    switch (state) {
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

QString languageName(const QString &languageId)
{
    if (languageId == QStringLiteral("javascript")) {
        return QStringLiteral("JavaScript");
    }
    if (languageId == QStringLiteral("lua")) {
        return QStringLiteral("Lua");
    }
    return languageId;
}

QString diagnosticsText(const QVector<ProcessorDiagnostic> &diagnostics)
{
    QStringList lines;
    lines.reserve(diagnostics.size());
    for (const ProcessorDiagnostic &diagnostic : diagnostics) {
        QString location;
        if (!diagnostic.file.isEmpty()) {
            location = diagnostic.file;
            if (diagnostic.line > 0) {
                location += QStringLiteral(":%1").arg(diagnostic.line);
                if (diagnostic.column > 0) {
                    location += QStringLiteral(":%1").arg(diagnostic.column);
                }
            }
        }
        const QString prefix = location.isEmpty()
            ? diagnostic.code
            : QStringLiteral("%1 · %2").arg(diagnostic.code, location);
        lines.append(prefix.isEmpty()
            ? diagnostic.message
            : QStringLiteral("%1: %2").arg(prefix, diagnostic.message));
    }
    return lines.join(QLatin1Char('\n'));
}

QString revisionSourceText(const ProcessorRevisionSnapshot &revision)
{
    QStringList parts;
    parts.reserve(revision.files.size() * 2);
    for (const ProcessorSourceFile &file : revision.files) {
        parts.append(file.path);
        parts.append(QString::fromUtf8(file.content));
    }
    return parts.join(QLatin1Char('\n'));
}

} // namespace

ProcessorLibraryModel::ProcessorLibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ProcessorLibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int ProcessorLibraryModel::count() const
{
    return rowCount();
}

QVariant ProcessorLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case IdRole:
        return row.id;
    case NameRole:
        return row.name;
    case DescriptionRole:
        return row.description;
    case LanguageIdRole:
        return row.languageId;
    case LanguageNameRole:
        return row.languageName;
    case RuntimeIdRole:
        return row.runtimeId;
    case ReadinessStateRole:
        return row.readinessState;
    case ReadinessDetailRole:
        return row.readinessDetail;
    case UpdatedAtRole:
        return row.updatedAt;
    case SourceTextRole:
        return row.sourceText;
    default:
        return {};
    }
}

QHash<int, QByteArray> ProcessorLibraryModel::roleNames() const
{
    static const QHash<int, QByteArray> roles {
        {IdRole, "id"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {LanguageIdRole, "languageId"},
        {LanguageNameRole, "languageName"},
        {RuntimeIdRole, "runtimeId"},
        {ReadinessStateRole, "readinessState"},
        {ReadinessDetailRole, "readinessDetail"},
        {UpdatedAtRole, "updatedAt"},
        {SourceTextRole, "sourceText"},
    };
    return roles;
}

QVariantMap ProcessorLibraryModel::rowAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? rowToMap(m_rows.at(row)) : QVariantMap {};
}

int ProcessorLibraryModel::indexOfId(const QString &id) const
{
    for (qsizetype row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row).id == id) {
            return static_cast<int>(row);
        }
    }
    return -1;
}

void ProcessorLibraryModel::refresh(
    ProcessorLibrary &library,
    MessageProcessorEngine &engine)
{
    QVector<Row> rows;
    const QVector<ProcessorDefinition> processors = library.processors();
    rows.reserve(processors.size());
    for (const ProcessorDefinition &processor : processors) {
        Row row;
        row.id = processor.id;
        row.name = processor.name;
        row.description = processor.description;
        row.updatedAt = displayTimestamp(processor.updatedAt);

        const auto revision = library.revisionById(processor.currentRevisionId);
        if (revision) {
            const ProcessorValidationResult validation = engine.validate(*revision);
            row.languageId = revision->languageId;
            row.languageName = languageName(revision->languageId);
            row.runtimeId = revision->runtimeId;
            row.readinessState = validationStateName(validation.state);
            row.readinessDetail = diagnosticsText(validation.diagnostics);
            row.sourceText = revisionSourceText(*revision);
        } else {
            row.readinessState = QStringLiteral("revision_not_found");
            row.readinessDetail = QStringLiteral("The Message Processor is unavailable.");
        }
        rows.append(std::move(row));
    }

    const bool rowCountChanged = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    if (rowCountChanged) {
        emit countChanged();
    }
}

QVariantMap ProcessorLibraryModel::rowToMap(const Row &row)
{
    return {
        {QStringLiteral("id"), row.id},
        {QStringLiteral("name"), row.name},
        {QStringLiteral("description"), row.description},
        {QStringLiteral("languageId"), row.languageId},
        {QStringLiteral("languageName"), row.languageName},
        {QStringLiteral("runtimeId"), row.runtimeId},
        {QStringLiteral("readinessState"), row.readinessState},
        {QStringLiteral("readinessDetail"), row.readinessDetail},
        {QStringLiteral("updatedAt"), row.updatedAt},
        {QStringLiteral("sourceText"), row.sourceText},
    };
}
