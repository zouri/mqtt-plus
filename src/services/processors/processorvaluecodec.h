#pragma once

#include <QByteArray>
#include <QCborValue>

namespace ProcessorValueCodec {

QCborValue canonicalize(const QCborValue &value);
QByteArray encodeCanonical(const QCborValue &value);

} // namespace ProcessorValueCodec
