#include "presentation/codesyntaxhighlighter.h"

#include <QColor>
#include <QElapsedTimer>
#include <QFont>
#include <QQuickTextDocument>
#include <QSet>
#include <QTextDocument>

namespace {

constexpr int kJavaScriptBlockCommentState = 1;
constexpr int kJavaScriptTemplateStringState = 2;
constexpr int kLuaBlockCommentState = 3;

const QSet<QString> &javaScriptKeywords()
{
    static const QSet<QString> keywords {
        QStringLiteral("async"), QStringLiteral("await"), QStringLiteral("break"),
        QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"),
        QStringLiteral("const"), QStringLiteral("continue"), QStringLiteral("debugger"),
        QStringLiteral("default"), QStringLiteral("delete"), QStringLiteral("do"),
        QStringLiteral("else"), QStringLiteral("export"), QStringLiteral("extends"),
        QStringLiteral("false"), QStringLiteral("finally"), QStringLiteral("for"),
        QStringLiteral("function"), QStringLiteral("if"), QStringLiteral("import"),
        QStringLiteral("in"), QStringLiteral("instanceof"), QStringLiteral("let"),
        QStringLiteral("new"), QStringLiteral("null"), QStringLiteral("return"),
        QStringLiteral("static"), QStringLiteral("super"), QStringLiteral("switch"),
        QStringLiteral("this"), QStringLiteral("throw"), QStringLiteral("true"),
        QStringLiteral("try"), QStringLiteral("typeof"), QStringLiteral("undefined"),
        QStringLiteral("var"), QStringLiteral("void"), QStringLiteral("while"),
        QStringLiteral("with"), QStringLiteral("yield"),
    };
    return keywords;
}

const QSet<QString> &javaScriptBuiltins()
{
    static const QSet<QString> builtins {
        QStringLiteral("Array"), QStringLiteral("BigInt"), QStringLiteral("Boolean"),
        QStringLiteral("Date"), QStringLiteral("Error"), QStringLiteral("JSON"),
        QStringLiteral("Map"), QStringLiteral("Math"), QStringLiteral("Number"),
        QStringLiteral("Object"), QStringLiteral("Promise"), QStringLiteral("RegExp"),
        QStringLiteral("Set"), QStringLiteral("String"), QStringLiteral("console"),
    };
    return builtins;
}

const QSet<QString> &luaKeywords()
{
    static const QSet<QString> keywords {
        QStringLiteral("and"), QStringLiteral("break"), QStringLiteral("do"),
        QStringLiteral("else"), QStringLiteral("elseif"), QStringLiteral("end"),
        QStringLiteral("false"), QStringLiteral("for"), QStringLiteral("function"),
        QStringLiteral("goto"), QStringLiteral("if"), QStringLiteral("in"),
        QStringLiteral("local"), QStringLiteral("nil"), QStringLiteral("not"),
        QStringLiteral("or"), QStringLiteral("repeat"), QStringLiteral("return"),
        QStringLiteral("then"), QStringLiteral("true"), QStringLiteral("until"),
        QStringLiteral("while"),
    };
    return keywords;
}

const QSet<QString> &luaBuiltins()
{
    static const QSet<QString> builtins {
        QStringLiteral("assert"), QStringLiteral("error"), QStringLiteral("ipairs"),
        QStringLiteral("math"), QStringLiteral("next"), QStringLiteral("pairs"),
        QStringLiteral("pcall"), QStringLiteral("select"), QStringLiteral("string"),
        QStringLiteral("table"), QStringLiteral("tonumber"), QStringLiteral("tostring"),
        QStringLiteral("type"), QStringLiteral("utf8"), QStringLiteral("xpcall"),
    };
    return builtins;
}

bool isIdentifierStart(QChar character, bool javascript)
{
    return character == QLatin1Char('_')
        || (javascript && character == QLatin1Char('$'))
        || character.isLetter();
}

bool isIdentifierPart(QChar character, bool javascript)
{
    return isIdentifierStart(character, javascript) || character.isNumber();
}

int quotedStringEnd(const QString &text, int start, QChar quote)
{
    bool escaped = false;
    for (int index = start + 1; index < text.size(); ++index) {
        const QChar character = text.at(index);
        if (!escaped && character == quote) {
            return index + 1;
        }
        if (!escaped && character == QLatin1Char('\\')) {
            escaped = true;
        } else {
            escaped = false;
        }
    }
    return text.size();
}

int numberEnd(const QString &text, int start)
{
    int index = start;
    if (text.mid(start, 2).compare(QStringLiteral("0x"), Qt::CaseInsensitive) == 0) {
        index += 2;
        while (index < text.size()
               && (text.at(index).isDigit()
                   || (text.at(index).toLower() >= QLatin1Char('a')
                       && text.at(index).toLower() <= QLatin1Char('f')))) {
            ++index;
        }
        return index;
    }

    bool hasDecimalPoint = false;
    bool hasExponent = false;
    while (index < text.size()) {
        const QChar character = text.at(index);
        if (character.isDigit()) {
            ++index;
            continue;
        }
        if (!hasDecimalPoint && !hasExponent && character == QLatin1Char('.')) {
            hasDecimalPoint = true;
            ++index;
            continue;
        }
        if (!hasExponent
            && (character == QLatin1Char('e') || character == QLatin1Char('E'))) {
            hasExponent = true;
            ++index;
            if (index < text.size()
                && (text.at(index) == QLatin1Char('+')
                    || text.at(index) == QLatin1Char('-'))) {
                ++index;
            }
            continue;
        }
        break;
    }
    return index;
}

QTextCharFormat syntaxFormat(const QColor &color, bool bold = false, bool italic = false)
{
    QTextCharFormat result;
    result.setForeground(color);
    result.setFontWeight(bold ? QFont::DemiBold : QFont::Normal);
    result.setFontItalic(italic);
    return result;
}

} // namespace

CodeSyntaxHighlighter::CodeSyntaxHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent)
{
    rebuildFormats();
    m_rehighlightTimer.setSingleShot(true);
    connect(&m_rehighlightTimer, &QTimer::timeout,
            this, &CodeSyntaxHighlighter::rehighlightNextChunk);
}

QQuickTextDocument *CodeSyntaxHighlighter::textDocument() const
{
    return m_textDocument;
}

QString CodeSyntaxHighlighter::language() const
{
    return m_language;
}

bool CodeSyntaxHighlighter::darkTheme() const
{
    return m_darkTheme;
}

void CodeSyntaxHighlighter::setTextDocument(QQuickTextDocument *document)
{
    if (document == m_textDocument) {
        return;
    }
    m_rehighlightTimer.stop();
    m_nextBlock = {};
    m_highlightingEnabled = false;
    m_textDocument = document;
    setDocument(document ? document->textDocument() : nullptr);
    QTimer::singleShot(0, this, [this]() {
        m_highlightingEnabled = true;
        scheduleRehighlight();
    });
    emit textDocumentChanged();
}

void CodeSyntaxHighlighter::setLanguage(const QString &language)
{
    QString normalized = language.trimmed().toLower();
    if (normalized == QStringLiteral("js")) {
        normalized = QStringLiteral("javascript");
    }
    if (normalized != QStringLiteral("javascript")
        && normalized != QStringLiteral("lua")) {
        normalized.clear();
    }
    if (normalized == m_language) {
        return;
    }
    m_language = normalized;
    scheduleRehighlight();
    emit languageChanged();
}

void CodeSyntaxHighlighter::setDarkTheme(bool darkTheme)
{
    if (darkTheme == m_darkTheme) {
        return;
    }
    m_darkTheme = darkTheme;
    rebuildFormats();
    scheduleRehighlight();
    emit darkThemeChanged();
}

void CodeSyntaxHighlighter::highlightBlock(const QString &text)
{
    setCurrentBlockState(-1);
    if (!m_highlightingEnabled) {
        return;
    }
    if (m_language == QStringLiteral("javascript")) {
        highlightJavaScript(text);
    } else if (m_language == QStringLiteral("lua")) {
        highlightLua(text);
    }
}

void CodeSyntaxHighlighter::scheduleRehighlight()
{
    if (!m_highlightingEnabled || !document()) {
        return;
    }
    m_nextBlock = document()->firstBlock();
    m_rehighlightTimer.start(0);
}

void CodeSyntaxHighlighter::rehighlightNextChunk()
{
    if (!m_highlightingEnabled || !document()
        || !m_nextBlock.isValid() || m_nextBlock.document() != document()) {
        m_nextBlock = {};
        return;
    }

    QElapsedTimer timer;
    timer.start();
    do {
        const QTextBlock block = m_nextBlock;
        m_nextBlock = block.next();
        rehighlightBlock(block);
    } while (m_nextBlock.isValid() && timer.elapsed() < 4);

    if (m_nextBlock.isValid()) {
        m_rehighlightTimer.start(0);
    }
}

void CodeSyntaxHighlighter::rebuildFormats()
{
    m_keywordFormat = syntaxFormat(m_darkTheme ? QColor(QStringLiteral("#c4a7e7"))
                                               : QColor(QStringLiteral("#6d28d9")), true);
    m_stringFormat = syntaxFormat(m_darkTheme ? QColor(QStringLiteral("#9ccfd8"))
                                              : QColor(QStringLiteral("#0f766e")));
    m_numberFormat = syntaxFormat(m_darkTheme ? QColor(QStringLiteral("#f6c177"))
                                              : QColor(QStringLiteral("#a84f00")));
    m_commentFormat = syntaxFormat(m_darkTheme ? QColor(QStringLiteral("#82909b"))
                                               : QColor(QStringLiteral("#6f757e")), false, true);
    m_functionFormat = syntaxFormat(m_darkTheme ? QColor(QStringLiteral("#7aa2f7"))
                                                : QColor(QStringLiteral("#245bdb")));
    m_builtinFormat = syntaxFormat(m_darkTheme ? QColor(QStringLiteral("#eb6f92"))
                                               : QColor(QStringLiteral("#b42362")));
}

void CodeSyntaxHighlighter::formatIdentifier(
    const QString &text,
    int start,
    int length,
    bool javascript)
{
    const QString token = text.mid(start, length);
    const QSet<QString> &keywords = javascript ? javaScriptKeywords() : luaKeywords();
    const QSet<QString> &builtins = javascript ? javaScriptBuiltins() : luaBuiltins();
    if (keywords.contains(token)) {
        setFormat(start, length, m_keywordFormat);
        return;
    }
    if (builtins.contains(token)) {
        setFormat(start, length, m_builtinFormat);
        return;
    }

    int next = start + length;
    while (next < text.size() && text.at(next).isSpace()) {
        ++next;
    }
    if (next < text.size() && text.at(next) == QLatin1Char('(')) {
        setFormat(start, length, m_functionFormat);
    }
}

void CodeSyntaxHighlighter::highlightJavaScript(const QString &text)
{
    int index = 0;
    if (previousBlockState() == kJavaScriptBlockCommentState) {
        const int end = text.indexOf(QStringLiteral("*/"));
        if (end < 0) {
            setFormat(0, text.size(), m_commentFormat);
            setCurrentBlockState(kJavaScriptBlockCommentState);
            return;
        }
        setFormat(0, end + 2, m_commentFormat);
        index = end + 2;
    } else if (previousBlockState() == kJavaScriptTemplateStringState) {
        const int end = quotedStringEnd(text, -1, QLatin1Char('`'));
        setFormat(0, end, m_stringFormat);
        if (end == text.size()
            && (text.isEmpty() || text.back() != QLatin1Char('`'))) {
            setCurrentBlockState(kJavaScriptTemplateStringState);
            return;
        }
        index = end;
    }

    while (index < text.size()) {
        if (text.mid(index, 2) == QStringLiteral("//")) {
            setFormat(index, text.size() - index, m_commentFormat);
            return;
        }
        if (text.mid(index, 2) == QStringLiteral("/*")) {
            const int end = text.indexOf(QStringLiteral("*/"), index + 2);
            if (end < 0) {
                setFormat(index, text.size() - index, m_commentFormat);
                setCurrentBlockState(kJavaScriptBlockCommentState);
                return;
            }
            setFormat(index, end + 2 - index, m_commentFormat);
            index = end + 2;
            continue;
        }

        const QChar character = text.at(index);
        if (character == QLatin1Char('\'') || character == QLatin1Char('"')
            || character == QLatin1Char('`')) {
            const int end = quotedStringEnd(text, index, character);
            setFormat(index, end - index, m_stringFormat);
            if (character == QLatin1Char('`') && end == text.size()
                && text.back() != QLatin1Char('`')) {
                setCurrentBlockState(kJavaScriptTemplateStringState);
                return;
            }
            index = end;
            continue;
        }
        if (character.isDigit()) {
            const int end = numberEnd(text, index);
            setFormat(index, end - index, m_numberFormat);
            index = end;
            continue;
        }
        if (isIdentifierStart(character, true)) {
            int end = index + 1;
            while (end < text.size() && isIdentifierPart(text.at(end), true)) {
                ++end;
            }
            formatIdentifier(text, index, end - index, true);
            index = end;
            continue;
        }
        ++index;
    }
}

void CodeSyntaxHighlighter::highlightLua(const QString &text)
{
    int index = 0;
    if (previousBlockState() == kLuaBlockCommentState) {
        const int end = text.indexOf(QStringLiteral("]]"));
        if (end < 0) {
            setFormat(0, text.size(), m_commentFormat);
            setCurrentBlockState(kLuaBlockCommentState);
            return;
        }
        setFormat(0, end + 2, m_commentFormat);
        index = end + 2;
    }

    while (index < text.size()) {
        if (text.mid(index, 4) == QStringLiteral("--[[")) {
            const int end = text.indexOf(QStringLiteral("]]"), index + 4);
            if (end < 0) {
                setFormat(index, text.size() - index, m_commentFormat);
                setCurrentBlockState(kLuaBlockCommentState);
                return;
            }
            setFormat(index, end + 2 - index, m_commentFormat);
            index = end + 2;
            continue;
        }
        if (text.mid(index, 2) == QStringLiteral("--")) {
            setFormat(index, text.size() - index, m_commentFormat);
            return;
        }

        const QChar character = text.at(index);
        if (character == QLatin1Char('\'') || character == QLatin1Char('"')) {
            const int end = quotedStringEnd(text, index, character);
            setFormat(index, end - index, m_stringFormat);
            index = end;
            continue;
        }
        if (character.isDigit()) {
            const int end = numberEnd(text, index);
            setFormat(index, end - index, m_numberFormat);
            index = end;
            continue;
        }
        if (isIdentifierStart(character, false)) {
            int end = index + 1;
            while (end < text.size() && isIdentifierPart(text.at(end), false)) {
                ++end;
            }
            formatIdentifier(text, index, end - index, false);
            index = end;
            continue;
        }
        ++index;
    }
}
