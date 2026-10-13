#pragma once

#include "services/processors/processorlibrary.h"
#include <QJsonObject>

namespace EditorCodec {
QJsonObject package(const SaveProcessorRevisionCommand &command);
bool command(const QJsonObject &json, SaveProcessorRevisionCommand &command, QString &error, bool requireName = true);
QJsonObject processor(const ProcessorDefinition &definition,
    const QSharedPointer<const ProcessorRevisionSnapshot> &revision = {});
QJsonObject templateDraft(const QString &language);
int runValidator(int argc, char **argv);
}
