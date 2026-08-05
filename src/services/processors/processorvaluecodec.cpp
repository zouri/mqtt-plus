#include "processorvaluecodec.h"

#include <QCborArray>
#include <QCborMap>
#include <QVector>

#include <algorithm>

namespace {

struct CanonicalMapEntry
{
    QCborValue key;
    QCborValue value;
    QByteArray encodedKey;
};

} // namespace

namespace ProcessorValueCodec {

QCborValue canonicalize(const QCborValue &value)
{
    if (value.isArray()) {
        QCborArray result;
        const QCborArray source = value.toArray();
        for (const QCborValue &item : source) {
            result.append(canonicalize(item));
        }
        return result;
    }
    if (value.isMap()) {
        QVector<CanonicalMapEntry> entries;
        const QCborMap source = value.toMap();
        entries.reserve(source.size());
        for (auto it = source.constBegin(); it != source.constEnd(); ++it) {
            const QCborValue key = canonicalize(it.key());
            entries.append({
                key,
                canonicalize(it.value()),
                key.toCbor(),
            });
        }
        std::sort(
            entries.begin(),
            entries.end(),
            [](const CanonicalMapEntry &left, const CanonicalMapEntry &right) {
                if (left.encodedKey.size() != right.encodedKey.size()) {
                    return left.encodedKey.size() < right.encodedKey.size();
                }
                return left.encodedKey < right.encodedKey;
            });

        QCborMap result;
        for (const CanonicalMapEntry &entry : entries) {
            result.insert(entry.key, entry.value);
        }
        return result;
    }
    return value;
}

QByteArray encodeCanonical(const QCborValue &value)
{
    return canonicalize(value).toCbor();
}

} // namespace ProcessorValueCodec
