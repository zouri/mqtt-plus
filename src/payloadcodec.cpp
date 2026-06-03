#include "payloadcodec.h"

#include <QByteArrayView>
#include <QCborValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QVariant>
#include <QtEndian>

#include <limits>

namespace {
constexpr qsizetype kMaxStructuredPayloadBytes = 16 * 1024 * 1024;
constexpr int kMaxMsgPackDepth = 32;
constexpr quint32 kMaxMsgPackStringBytes = 8 * 1024 * 1024;
constexpr quint32 kMaxMsgPackContainerItems = 100000;

QString formatErrorText(const QString &label, const QString &error)
{
    return QStringLiteral("[%1 parse error] %2").arg(label, error);
}

bool isTooLarge(qsizetype size)
{
    return size > kMaxStructuredPayloadBytes;
}

QJsonValue toJsonValue(const QVariant &value)
{
    return QJsonValue::fromVariant(value);
}

QString variantToPrettyJson(const QVariant &value)
{
    const QJsonValue jsonValue = toJsonValue(value);
    if (jsonValue.isArray()) {
        return QString::fromUtf8(QJsonDocument(jsonValue.toArray()).toJson(QJsonDocument::Indented)).trimmed();
    }
    if (jsonValue.isObject()) {
        return QString::fromUtf8(QJsonDocument(jsonValue.toObject()).toJson(QJsonDocument::Indented)).trimmed();
    }
    if (jsonValue.isString()) {
        return jsonValue.toString();
    }
    if (jsonValue.isBool()) {
        return jsonValue.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (jsonValue.isNull()) {
        return QStringLiteral("null");
    }
    if (jsonValue.isDouble()) {
        return QString::number(jsonValue.toDouble(), 'g', 16);
    }
    return QStringLiteral("null");
}

bool appendMsgPackValue(QByteArray &buffer, const QVariant &value, QString &error);

void appendU8(QByteArray &buffer, quint8 value)
{
    buffer.append(static_cast<char>(value));
}

void appendU16(QByteArray &buffer, quint16 value)
{
    const quint16 be = qToBigEndian(value);
    buffer.append(reinterpret_cast<const char *>(&be), sizeof(be));
}

void appendU32(QByteArray &buffer, quint32 value)
{
    const quint32 be = qToBigEndian(value);
    buffer.append(reinterpret_cast<const char *>(&be), sizeof(be));
}

void appendU64(QByteArray &buffer, quint64 value)
{
    const quint64 be = qToBigEndian(value);
    buffer.append(reinterpret_cast<const char *>(&be), sizeof(be));
}

bool appendMsgPackInt(QByteArray &buffer, qint64 value)
{
    if (value >= -32 && value <= 127) {
        appendU8(buffer, static_cast<quint8>(static_cast<qint8>(value)));
        return true;
    }
    if (value >= (std::numeric_limits<qint8>::min)() && value <= (std::numeric_limits<qint8>::max)()) {
        appendU8(buffer, 0xD0);
        appendU8(buffer, static_cast<quint8>(static_cast<qint8>(value)));
        return true;
    }
    if (value >= (std::numeric_limits<qint16>::min)() && value <= (std::numeric_limits<qint16>::max)()) {
        appendU8(buffer, 0xD1);
        appendU16(buffer, static_cast<quint16>(static_cast<qint16>(value)));
        return true;
    }
    if (value >= (std::numeric_limits<qint32>::min)() && value <= (std::numeric_limits<qint32>::max)()) {
        appendU8(buffer, 0xD2);
        appendU32(buffer, static_cast<quint32>(static_cast<qint32>(value)));
        return true;
    }
    appendU8(buffer, 0xD3);
    appendU64(buffer, static_cast<quint64>(value));
    return true;
}

bool appendMsgPackUInt(QByteArray &buffer, quint64 value)
{
    if (value <= 127) {
        appendU8(buffer, static_cast<quint8>(value));
        return true;
    }
    if (value <= (std::numeric_limits<quint8>::max)()) {
        appendU8(buffer, 0xCC);
        appendU8(buffer, static_cast<quint8>(value));
        return true;
    }
    if (value <= (std::numeric_limits<quint16>::max)()) {
        appendU8(buffer, 0xCD);
        appendU16(buffer, static_cast<quint16>(value));
        return true;
    }
    if (value <= (std::numeric_limits<quint32>::max)()) {
        appendU8(buffer, 0xCE);
        appendU32(buffer, static_cast<quint32>(value));
        return true;
    }
    appendU8(buffer, 0xCF);
    appendU64(buffer, value);
    return true;
}

bool appendMsgPackString(QByteArray &buffer, const QString &value, QString &error)
{
    const QByteArray utf8 = value.toUtf8();
    if (isTooLarge(utf8.size())) {
        error = QStringLiteral("MsgPack string exceeds the maximum supported size.");
        return false;
    }
    const quint32 len = static_cast<quint32>(utf8.size());
    if (len <= 31) {
        appendU8(buffer, static_cast<quint8>(0xA0 | len));
    } else if (len <= (std::numeric_limits<quint8>::max)()) {
        appendU8(buffer, 0xD9);
        appendU8(buffer, static_cast<quint8>(len));
    } else if (len <= (std::numeric_limits<quint16>::max)()) {
        appendU8(buffer, 0xDA);
        appendU16(buffer, static_cast<quint16>(len));
    } else {
        appendU8(buffer, 0xDB);
        appendU32(buffer, len);
    }
    buffer.append(utf8);
    return true;
}

bool appendMsgPackArray(QByteArray &buffer, const QVariantList &array, QString &error)
{
    if (array.size() > static_cast<qsizetype>(kMaxMsgPackContainerItems)) {
        error = QStringLiteral("MsgPack array exceeds the maximum item count.");
        return false;
    }
    const quint32 len = static_cast<quint32>(array.size());
    if (len <= 15) {
        appendU8(buffer, static_cast<quint8>(0x90 | len));
    } else if (len <= (std::numeric_limits<quint16>::max)()) {
        appendU8(buffer, 0xDC);
        appendU16(buffer, static_cast<quint16>(len));
    } else {
        appendU8(buffer, 0xDD);
        appendU32(buffer, len);
    }
    for (const QVariant &item : array) {
        if (!appendMsgPackValue(buffer, item, error)) {
            return false;
        }
    }
    return true;
}

bool appendMsgPackMap(QByteArray &buffer, const QVariantMap &map, QString &error)
{
    if (map.size() > static_cast<qsizetype>(kMaxMsgPackContainerItems)) {
        error = QStringLiteral("MsgPack map exceeds the maximum item count.");
        return false;
    }
    const quint32 len = static_cast<quint32>(map.size());
    if (len <= 15) {
        appendU8(buffer, static_cast<quint8>(0x80 | len));
    } else if (len <= (std::numeric_limits<quint16>::max)()) {
        appendU8(buffer, 0xDE);
        appendU16(buffer, static_cast<quint16>(len));
    } else {
        appendU8(buffer, 0xDF);
        appendU32(buffer, len);
    }
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if (!appendMsgPackString(buffer, it.key(), error)) {
            return false;
        }
        if (!appendMsgPackValue(buffer, it.value(), error)) {
            return false;
        }
    }
    return true;
}

bool appendMsgPackValue(QByteArray &buffer, const QVariant &value, QString &error)
{
    if (!value.isValid() || value.isNull()) {
        appendU8(buffer, 0xC0);
        return true;
    }

    switch (value.userType()) {
    case QMetaType::Bool:
        appendU8(buffer, value.toBool() ? 0xC3 : 0xC2);
        return true;
    case QMetaType::Int:
    case QMetaType::LongLong:
        return appendMsgPackInt(buffer, value.toLongLong());
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        return appendMsgPackUInt(buffer, value.toULongLong());
    case QMetaType::Double: {
        appendU8(buffer, 0xCB);
        union {
            double d;
            quint64 u;
        } v;
        v.d = value.toDouble();
        appendU64(buffer, v.u);
        return true;
    }
    case QMetaType::QString:
        return appendMsgPackString(buffer, value.toString(), error);
    case QMetaType::QVariantList:
        return appendMsgPackArray(buffer, value.toList(), error);
    case QMetaType::QVariantMap:
        return appendMsgPackMap(buffer, value.toMap(), error);
    default:
        if (value.canConvert<QVariantList>()) {
            return appendMsgPackArray(buffer, value.toList(), error);
        }
        if (value.canConvert<QVariantMap>()) {
            return appendMsgPackMap(buffer, value.toMap(), error);
        }
        error = QStringLiteral("Unsupported MsgPack type: %1").arg(QString::fromLatin1(value.typeName()));
        return false;
    }
}

bool readBytes(QByteArrayView data, int &offset, int count, QByteArrayView &out)
{
    if (count < 0 || offset < 0 || offset > data.size() || count > data.size() - offset) {
        return false;
    }
    out = data.sliced(offset, count);
    offset += count;
    return true;
}

bool readU8(QByteArrayView data, int &offset, quint8 &out)
{
    if (offset >= data.size()) {
        return false;
    }
    out = static_cast<quint8>(data.at(offset));
    ++offset;
    return true;
}

bool readU16(QByteArrayView data, int &offset, quint16 &out)
{
    QByteArrayView bytes;
    if (!readBytes(data, offset, 2, bytes)) {
        return false;
    }
    out = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(bytes.data()));
    return true;
}

bool readU32(QByteArrayView data, int &offset, quint32 &out)
{
    QByteArrayView bytes;
    if (!readBytes(data, offset, 4, bytes)) {
        return false;
    }
    out = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(bytes.data()));
    return true;
}

bool readU64(QByteArrayView data, int &offset, quint64 &out)
{
    QByteArrayView bytes;
    if (!readBytes(data, offset, 8, bytes)) {
        return false;
    }
    out = qFromBigEndian<quint64>(reinterpret_cast<const uchar *>(bytes.data()));
    return true;
}

bool parseMsgPackValue(QByteArrayView data, int &offset, QVariant &out, QString &error, int depth);

bool parseMsgPackString(QByteArrayView data, int &offset, quint32 len, QVariant &out, QString &error)
{
    if (len > kMaxMsgPackStringBytes) {
        error = QStringLiteral("MsgPack string exceeds the maximum length.");
        return false;
    }
    if (len > static_cast<quint32>((std::numeric_limits<int>::max)())) {
        error = QStringLiteral("MsgPack string length is too large.");
        return false;
    }
    QByteArrayView bytes;
    if (!readBytes(data, offset, static_cast<int>(len), bytes)) {
        error = QStringLiteral("Unexpected end of MsgPack string.");
        return false;
    }
    out = QString::fromUtf8(bytes.data(), bytes.size());
    return true;
}

bool parseMsgPackArray(QByteArrayView data, int &offset, quint32 len, QVariant &out, QString &error, int depth)
{
    if (len > kMaxMsgPackContainerItems) {
        error = QStringLiteral("MsgPack array exceeds the maximum item count.");
        return false;
    }
    QVariantList list;
    list.reserve(static_cast<int>(len));
    for (quint32 i = 0; i < len; ++i) {
        QVariant item;
        if (!parseMsgPackValue(data, offset, item, error, depth + 1)) {
            return false;
        }
        list.append(item);
    }
    out = list;
    return true;
}

bool parseMsgPackMap(QByteArrayView data, int &offset, quint32 len, QVariant &out, QString &error, int depth)
{
    if (len > kMaxMsgPackContainerItems) {
        error = QStringLiteral("MsgPack map exceeds the maximum item count.");
        return false;
    }
    QVariantMap map;
    for (quint32 i = 0; i < len; ++i) {
        QVariant keyVar;
        if (!parseMsgPackValue(data, offset, keyVar, error, depth + 1)) {
            return false;
        }
        if (keyVar.userType() != QMetaType::QString) {
            error = QStringLiteral("MsgPack map key is not a string.");
            return false;
        }

        QVariant valueVar;
        if (!parseMsgPackValue(data, offset, valueVar, error, depth + 1)) {
            return false;
        }
        map.insert(keyVar.toString(), valueVar);
    }
    out = map;
    return true;
}

bool parseMsgPackValue(QByteArrayView data, int &offset, QVariant &out, QString &error, int depth)
{
    if (depth > kMaxMsgPackDepth) {
        error = QStringLiteral("MsgPack nesting is too deep.");
        return false;
    }

    quint8 marker = 0;
    if (!readU8(data, offset, marker)) {
        error = QStringLiteral("Unexpected end of MsgPack payload.");
        return false;
    }

    if (marker <= 0x7F) {
        out = static_cast<qulonglong>(marker);
        return true;
    }
    if (marker >= 0xE0) {
        out = static_cast<qlonglong>(static_cast<qint8>(marker));
        return true;
    }
    if ((marker & 0xE0) == 0xA0) {
        return parseMsgPackString(data, offset, marker & 0x1F, out, error);
    }
    if ((marker & 0xF0) == 0x90) {
        return parseMsgPackArray(data, offset, marker & 0x0F, out, error, depth);
    }
    if ((marker & 0xF0) == 0x80) {
        return parseMsgPackMap(data, offset, marker & 0x0F, out, error, depth);
    }

    switch (marker) {
    case 0xC0:
        out = QVariant();
        return true;
    case 0xC2:
        out = false;
        return true;
    case 0xC3:
        out = true;
        return true;
    case 0xCC: {
        quint8 v = 0;
        if (!readU8(data, offset, v)) {
            error = QStringLiteral("Invalid uint8.");
            return false;
        }
        out = static_cast<qulonglong>(v);
        return true;
    }
    case 0xCD: {
        quint16 v = 0;
        if (!readU16(data, offset, v)) {
            error = QStringLiteral("Invalid uint16.");
            return false;
        }
        out = static_cast<qulonglong>(v);
        return true;
    }
    case 0xCE: {
        quint32 v = 0;
        if (!readU32(data, offset, v)) {
            error = QStringLiteral("Invalid uint32.");
            return false;
        }
        out = static_cast<qulonglong>(v);
        return true;
    }
    case 0xCF: {
        quint64 v = 0;
        if (!readU64(data, offset, v)) {
            error = QStringLiteral("Invalid uint64.");
            return false;
        }
        out = static_cast<qulonglong>(v);
        return true;
    }
    case 0xD0: {
        quint8 v = 0;
        if (!readU8(data, offset, v)) {
            error = QStringLiteral("Invalid int8.");
            return false;
        }
        out = static_cast<qlonglong>(static_cast<qint8>(v));
        return true;
    }
    case 0xD1: {
        quint16 v = 0;
        if (!readU16(data, offset, v)) {
            error = QStringLiteral("Invalid int16.");
            return false;
        }
        out = static_cast<qlonglong>(static_cast<qint16>(v));
        return true;
    }
    case 0xD2: {
        quint32 v = 0;
        if (!readU32(data, offset, v)) {
            error = QStringLiteral("Invalid int32.");
            return false;
        }
        out = static_cast<qlonglong>(static_cast<qint32>(v));
        return true;
    }
    case 0xD3: {
        quint64 v = 0;
        if (!readU64(data, offset, v)) {
            error = QStringLiteral("Invalid int64.");
            return false;
        }
        out = static_cast<qlonglong>(static_cast<qint64>(v));
        return true;
    }
    case 0xCA: {
        quint32 v = 0;
        if (!readU32(data, offset, v)) {
            error = QStringLiteral("Invalid float32.");
            return false;
        }
        union {
            quint32 u;
            float f;
        } conv;
        conv.u = v;
        out = static_cast<double>(conv.f);
        return true;
    }
    case 0xCB: {
        quint64 v = 0;
        if (!readU64(data, offset, v)) {
            error = QStringLiteral("Invalid float64.");
            return false;
        }
        union {
            quint64 u;
            double d;
        } conv;
        conv.u = v;
        out = conv.d;
        return true;
    }
    case 0xD9: {
        quint8 len = 0;
        if (!readU8(data, offset, len)) {
            error = QStringLiteral("Invalid str8 length.");
            return false;
        }
        return parseMsgPackString(data, offset, len, out, error);
    }
    case 0xDA: {
        quint16 len = 0;
        if (!readU16(data, offset, len)) {
            error = QStringLiteral("Invalid str16 length.");
            return false;
        }
        return parseMsgPackString(data, offset, len, out, error);
    }
    case 0xDB: {
        quint32 len = 0;
        if (!readU32(data, offset, len)) {
            error = QStringLiteral("Invalid str32 length.");
            return false;
        }
        return parseMsgPackString(data, offset, len, out, error);
    }
    case 0xDC: {
        quint16 len = 0;
        if (!readU16(data, offset, len)) {
            error = QStringLiteral("Invalid array16 length.");
            return false;
        }
        return parseMsgPackArray(data, offset, len, out, error, depth);
    }
    case 0xDD: {
        quint32 len = 0;
        if (!readU32(data, offset, len)) {
            error = QStringLiteral("Invalid array32 length.");
            return false;
        }
        return parseMsgPackArray(data, offset, len, out, error, depth);
    }
    case 0xDE: {
        quint16 len = 0;
        if (!readU16(data, offset, len)) {
            error = QStringLiteral("Invalid map16 length.");
            return false;
        }
        return parseMsgPackMap(data, offset, len, out, error, depth);
    }
    case 0xDF: {
        quint32 len = 0;
        if (!readU32(data, offset, len)) {
            error = QStringLiteral("Invalid map32 length.");
            return false;
        }
        return parseMsgPackMap(data, offset, len, out, error, depth);
    }
    default:
        error = QStringLiteral("Unsupported MsgPack marker 0x%1").arg(QString::number(marker, 16));
        return false;
    }
}

int topicSpecificityScore(const QString &filter)
{
    int score = filter.count('/');
    for (const QChar character : filter) {
        if (character != QLatin1Char('#')
                && character != QLatin1Char('+')
                && character != QLatin1Char('/')) {
            ++score;
        }
    }
    return score;
}

QByteArray decodeHexString(const QString &value, QString &error)
{
    QString text = value;
    text.remove(QRegularExpression(QStringLiteral("\\s+")));
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        text = text.mid(2);
    }
    if (text.isEmpty()) {
        return {};
    }
    static const QRegularExpression kHexPattern(QStringLiteral("^[0-9A-Fa-f]+$"));
    if (!kHexPattern.match(text).hasMatch()) {
        error = QStringLiteral("Hex payload contains non-hex characters.");
        return {};
    }
    if ((text.size() % 2) != 0) {
        error = QStringLiteral("Hex payload length must be even.");
        return {};
    }
    return QByteArray::fromHex(text.toUtf8());
}

QByteArray decodeBase64String(const QString &value, QString &error)
{
    const auto result = QByteArray::fromBase64Encoding(
        value.trimmed().toUtf8(),
        QByteArray::AbortOnBase64DecodingErrors);
    if (result.decodingStatus != QByteArray::Base64DecodingStatus::Ok) {
        error = QStringLiteral("Invalid Base64 payload.");
        return {};
    }
    return result.decoded;
}

QByteArray encodeJsonToUtf8(const QString &input, QString &error)
{
    const QByteArray inputBytes = input.toUtf8();
    if (isTooLarge(inputBytes.size())) {
        error = QStringLiteral("Structured payload exceeds the maximum supported size.");
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(inputBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || (!document.isArray() && !document.isObject())) {
        error = parseError.errorString().isEmpty()
                    ? QStringLiteral("Expected JSON object or array.")
                    : parseError.errorString();
        return {};
    }
    return document.toJson(QJsonDocument::Compact);
}

QByteArray encodeCborFromJson(const QString &input, QString &error)
{
    const QByteArray inputBytes = input.toUtf8();
    if (isTooLarge(inputBytes.size())) {
        error = QStringLiteral("Structured payload exceeds the maximum supported size.");
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(inputBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || (!document.isArray() && !document.isObject())) {
        error = parseError.errorString().isEmpty()
                    ? QStringLiteral("Expected JSON object or array for CBOR.")
                    : parseError.errorString();
        return {};
    }
    const QJsonValue value = document.isArray() ? QJsonValue(document.array()) : QJsonValue(document.object());
    return QCborValue::fromJsonValue(value).toCbor();
}

QByteArray encodeMsgPackFromJson(const QString &input, QString &error)
{
    const QByteArray inputBytes = input.toUtf8();
    if (isTooLarge(inputBytes.size())) {
        error = QStringLiteral("Structured payload exceeds the maximum supported size.");
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(inputBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || (!document.isArray() && !document.isObject())) {
        error = parseError.errorString().isEmpty()
                    ? QStringLiteral("Expected JSON object or array for MsgPack.")
                    : parseError.errorString();
        return {};
    }

    QByteArray output;
    const QVariant root = document.toVariant();
    if (!appendMsgPackValue(output, root, error)) {
        return {};
    }
    return output;
}
}

QStringList PayloadCodec::formatNames()
{
    return {
        QStringLiteral("Plaintext"),
        QStringLiteral("JSON"),
        QStringLiteral("Base64"),
        QStringLiteral("Hex"),
        QStringLiteral("CBOR"),
        QStringLiteral("MsgPack")
    };
}

PayloadFormat PayloadCodec::formatFromInt(int value)
{
    switch (value) {
    case 1:
        return PayloadFormat::Json;
    case 2:
        return PayloadFormat::Base64;
    case 3:
        return PayloadFormat::Hex;
    case 4:
        return PayloadFormat::Cbor;
    case 5:
        return PayloadFormat::MsgPack;
    case 0:
    default:
        return PayloadFormat::Plaintext;
    }
}

QString PayloadCodec::formatName(PayloadFormat format)
{
    switch (format) {
    case PayloadFormat::Json:
        return QStringLiteral("JSON");
    case PayloadFormat::Base64:
        return QStringLiteral("Base64");
    case PayloadFormat::Hex:
        return QStringLiteral("Hex");
    case PayloadFormat::Cbor:
        return QStringLiteral("CBOR");
    case PayloadFormat::MsgPack:
        return QStringLiteral("MsgPack");
    case PayloadFormat::Plaintext:
    default:
        return QStringLiteral("Plaintext");
    }
}

bool PayloadCodec::encodeForPublish(
    PayloadFormat format,
    const QString &input,
    QByteArray &output,
    QString &error)
{
    output.clear();
    error.clear();

    switch (format) {
    case PayloadFormat::Plaintext:
        output = input.toUtf8();
        return true;
    case PayloadFormat::Json:
        output = encodeJsonToUtf8(input, error);
        return error.isEmpty();
    case PayloadFormat::Base64:
        output = decodeBase64String(input, error);
        return error.isEmpty();
    case PayloadFormat::Hex:
        output = decodeHexString(input, error);
        return error.isEmpty();
    case PayloadFormat::Cbor:
        output = encodeCborFromJson(input, error);
        return error.isEmpty();
    case PayloadFormat::MsgPack:
        output = encodeMsgPackFromJson(input, error);
        return error.isEmpty();
    default:
        error = QStringLiteral("Unsupported payload format.");
        return false;
    }
}

QString PayloadCodec::decodeForDisplay(
    PayloadFormat format,
    const QByteArray &payloadBytes,
    QString &error)
{
    error.clear();

    switch (format) {
    case PayloadFormat::Plaintext:
        return QString::fromUtf8(payloadBytes);

    case PayloadFormat::Json: {
        if (isTooLarge(payloadBytes.size())) {
            error = formatErrorText(QStringLiteral("JSON"), QStringLiteral("Payload exceeds the maximum supported size."));
            return error;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payloadBytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || (!document.isObject() && !document.isArray())) {
            error = formatErrorText(QStringLiteral("JSON"), parseError.errorString());
            return error;
        }
        return QString::fromUtf8(document.toJson(QJsonDocument::Indented)).trimmed();
    }

    case PayloadFormat::Base64:
        return QString::fromLatin1(payloadBytes.toBase64());

    case PayloadFormat::Hex:
        return QString::fromLatin1(payloadBytes.toHex(' ').toUpper());

    case PayloadFormat::Cbor: {
        if (isTooLarge(payloadBytes.size())) {
            error = formatErrorText(QStringLiteral("CBOR"), QStringLiteral("Payload exceeds the maximum supported size."));
            return error;
        }
        QCborParserError parserError = {};
        const QCborValue value = QCborValue::fromCbor(payloadBytes, &parserError);
        if (parserError.error != QCborError::NoError) {
            error = formatErrorText(
                QStringLiteral("CBOR"),
                QStringLiteral("CBOR parser error code: %1").arg(static_cast<int>(parserError.error)));
            return error;
        }
        return value.toDiagnosticNotation();
    }

    case PayloadFormat::MsgPack: {
        if (isTooLarge(payloadBytes.size())) {
            error = formatErrorText(QStringLiteral("MsgPack"), QStringLiteral("Payload exceeds the maximum supported size."));
            return error;
        }
        int offset = 0;
        QVariant value;
        if (!parseMsgPackValue(QByteArrayView(payloadBytes), offset, value, error, 0)) {
            error = formatErrorText(QStringLiteral("MsgPack"), error);
            return error;
        }
        if (offset != payloadBytes.size()) {
            error = formatErrorText(QStringLiteral("MsgPack"), QStringLiteral("Trailing bytes detected."));
            return error;
        }
        return variantToPrettyJson(value);
    }
    default:
        error = QStringLiteral("Unsupported payload format.");
        return error;
    }
}

bool PayloadCodec::topicFilterMatches(const QString &filter, const QString &topic)
{
    if (filter.isEmpty()) {
        return false;
    }
    if (filter == topic) {
        return true;
    }

    const QStringList f = filter.split('/');
    const QStringList t = topic.split('/');

    int fi = 0;
    int ti = 0;
    while (fi < f.size() && ti < t.size()) {
        const QString &token = f.at(fi);
        if (token == QStringLiteral("#")) {
            return fi == f.size() - 1;
        }
        if (token != QStringLiteral("+") && token != t.at(ti)) {
            return false;
        }
        ++fi;
        ++ti;
    }

    if (fi == f.size() && ti == t.size()) {
        return true;
    }
    if (fi == f.size() - 1 && f.at(fi) == QStringLiteral("#")) {
        return true;
    }
    return false;
}

PayloadFormat PayloadCodec::resolveTopicFormat(
    const QHash<QString, int> &topicFormats,
    const QString &topic)
{
    if (topic.isEmpty() || topicFormats.isEmpty()) {
        return PayloadFormat::Plaintext;
    }

    if (topicFormats.contains(topic)) {
        return formatFromInt(topicFormats.value(topic));
    }

    QString bestFilter;
    int bestScore = -1;
    for (auto it = topicFormats.constBegin(); it != topicFormats.constEnd(); ++it) {
        const QString &filter = it.key();
        if (!topicFilterMatches(filter, topic)) {
            continue;
        }
        const int score = topicSpecificityScore(filter);
        if (score > bestScore || (score == bestScore && (bestFilter.isEmpty() || filter < bestFilter))) {
            bestScore = score;
            bestFilter = filter;
        }
    }

    if (bestScore >= 0) {
        return formatFromInt(topicFormats.value(bestFilter));
    }
    return PayloadFormat::Plaintext;
}
