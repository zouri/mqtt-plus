#include "editorcodec.h"
#include "services/processors/defaultmessageprocessorengine.h"
#include "services/processors/messageprocessorengine.h"
#include "services/processors/processorpackagehash.h"

#include <QCoreApplication>
#include <QCborValue>
#include <QJsonArray>
#include <QtEndian>
#include <cstdio>
#ifdef Q_OS_WIN
#include <io.h>
#include <fcntl.h>
#endif

QJsonObject EditorCodec::package(const SaveProcessorRevisionCommand &command)
{
    QJsonArray files;
    for (const auto &file : command.content.files) {
        files.append(QJsonObject{{"path", file.path}, {"mediaType", file.mediaType},
            {"content", QString::fromUtf8(file.content)}});
    }
    return {{"name", command.name}, {"description", command.description},
        {"languageId", command.content.languageId}, {"runtimeId", command.content.runtimeId},
        {"entryFile", command.content.entryFile}, {"entrySymbol", command.content.entrySymbol},
        {"contractId", command.content.contractId},
        {"manifest", QCborValue(command.content.manifest).toJsonValue()}, {"files", files}};
}

bool EditorCodec::command(const QJsonObject &json, SaveProcessorRevisionCommand &command, QString &error, bool requireName)
{
    command.name = json.value("name").toString();
    command.description = json.value("description").toString();
    command.processorId = json.value("processorId").toString();
    command.expectedVersion = json.value("baseVersion").toString();
    command.requestId = json.value("requestId").toString();
    command.content.languageId = json.value("languageId").toString();
    command.content.runtimeId = json.value("runtimeId").toString();
    command.content.entryFile = json.value("entryFile").toString();
    command.content.entrySymbol = json.value("entrySymbol").toString(QStringLiteral("process"));
    command.content.contractId = json.value("contractId").toString(QStringLiteral("mqtt-plus.message-processor/v1"));
    command.content.manifest = QCborValue::fromJsonValue(json.value("manifest")).toMap();
    if ((requireName && command.name.trimmed().isEmpty()) || command.name.size() > 256 || command.description.size() > 4096
        || !json.value("files").isArray()
        || (command.content.languageId != "lua" && command.content.languageId != "javascript")
        || command.content.runtimeId != (command.content.languageId == "lua" ? "lua-5.5" : "qt-qjs")) {
        error = QStringLiteral("Invalid processor metadata or runtime.");
        return false;
    }
    for (const auto &value : json.value("files").toArray()) {
        const auto file = value.toObject();
        if (!file.value("content").isString() || !file.value("path").isString()) {
            error = QStringLiteral("Source files must contain text and a path.");
            return false;
        }
        command.content.files.append({file.value("path").toString(), file.value("mediaType").toString(),
            file.value("content").toString().toUtf8(), {}});
    }
    const auto prepared = ProcessorPackageHash::prepare(command.content);
    if (!prepared.ok) { error = prepared.error; return false; }
    command.content = prepared.content;
    return true;
}

QJsonObject EditorCodec::processor(const ProcessorDefinition &definition,
    const QSharedPointer<const ProcessorRevisionSnapshot> &revision)
{
    QJsonObject result{{"id", definition.id}, {"name", definition.name}, {"description", definition.description},
        {"version", QString::number(definition.changeSequence)}, {"revisionId", definition.currentRevisionId},
        {"createdAt", definition.createdAt}, {"updatedAt", definition.updatedAt}};
    if (revision) {
        SaveProcessorRevisionCommand command;
        command.name = definition.name;
        command.description = definition.description;
        command.content = {revision->contractId, revision->languageId, revision->runtimeId,
            revision->entryFile, revision->entrySymbol, revision->manifest, revision->files};
        result.insert("package", package(command));
        result.insert("revisionNumber", QString::number(revision->revisionNumber));
    }
    return result;
}

QJsonObject EditorCodec::templateDraft(const QString &language)
{
    const bool lua = language == "lua";
    SaveProcessorRevisionCommand command;
    command.name = lua ? QStringLiteral("New Lua Processor") : QStringLiteral("New JavaScript Processor");
    command.content.languageId = lua ? "lua" : "javascript";
    command.content.runtimeId = lua ? "lua-5.5" : "qt-qjs";
    command.content.entryFile = lua ? "main.lua" : "main.js";
    command.content.files.append({command.content.entryFile, lua ? "text/x-lua" : "text/javascript",
        lua ? QByteArray("function process(context)\n    return context.decoded\nend\n")
            : QByteArray("function process(context) {\n    return context.decoded;\n}\n"), {}});
    return package(command);
}

int EditorCodec::runValidator(int argc, char **argv)
{
#ifdef Q_OS_WIN
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    QCoreApplication application(argc, argv);
    quint32 header = 0;
    if (std::fread(&header, 1, 4, stdin) != 4) return 2;
    const auto size = qFromBigEndian(header);
    if (size > 4 * 1024 * 1024) return 2;
    QByteArray input(size, Qt::Uninitialized);
    if (std::fread(input.data(), 1, size, stdin) != size) return 2;
    QCborParserError parseError;
    const auto value = QCborValue::fromCbor(input, &parseError);
    if (parseError.error != QCborError::NoError || !value.isMap()) return 2;
    SaveProcessorRevisionCommand command;
    QString error;
    QJsonObject result;
    if (!EditorCodec::command(value.toJsonValue().toObject(), command, error)) {
        result = {{"ok", false}, {"diagnostics", QJsonArray{QJsonObject{{"code", "invalid_source"}, {"message", error}}}}};
    } else {
        const auto prepared = ProcessorPackageHash::prepare(command.content);
        ProcessorRevisionSnapshot revision{QStringLiteral("preview"), QStringLiteral("preview"), 1,
            command.content.contractId, command.content.languageId, command.content.runtimeId,
            command.content.entryFile, command.content.entrySymbol, command.content.manifest,
            prepared.contentHash, command.content.files, {}};
        auto engine = createDefaultMessageProcessorEngine();
        const auto validation = engine->validate(revision);
        QJsonArray diagnostics;
        for (const auto &item : validation.diagnostics) diagnostics.append(QJsonObject{
            {"code", item.code}, {"message", item.message}, {"file", item.file}, {"line", item.line}, {"column", item.column}});
        result = {{"ok", validation.isReady()}, {"diagnostics", diagnostics}};
    }
    const auto output = QCborValue::fromJsonValue(result).toCbor();
    header = qToBigEndian(quint32(output.size()));
    if (std::fwrite(&header, 1, 4, stdout) != 4
        || std::fwrite(output.constData(), 1, output.size(), stdout) != size_t(output.size())) return 2;
    std::fflush(stdout);
    return 0;
}
