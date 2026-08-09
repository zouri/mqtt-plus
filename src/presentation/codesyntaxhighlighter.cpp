#include "presentation/codesyntaxhighlighter.h"

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Theme>

#include <QElapsedTimer>
#include <QFont>
#include <QQuickTextDocument>
#include <QTextDocument>

#include <utility>

namespace {

KSyntaxHighlighting::Repository &syntaxRepository()
{
    static KSyntaxHighlighting::Repository repository;
    return repository;
}

} // namespace

CodeSyntaxHighlighter::CodeSyntaxHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent)
{
    updateTheme();
    m_rehighlightTimer.setSingleShot(true);
    connect(&m_rehighlightTimer, &QTimer::timeout,
            this, &CodeSyntaxHighlighter::rehighlightNextChunk);
    m_visibleRangeTimer.setSingleShot(true);
    connect(&m_visibleRangeTimer, &QTimer::timeout,
            this, &CodeSyntaxHighlighter::scheduleRehighlight);
    m_stateCacheTimer.setSingleShot(true);
    connect(&m_stateCacheTimer, &QTimer::timeout,
            this, &CodeSyntaxHighlighter::rebuildBlockStateCacheChunk);
}

CodeSyntaxHighlighter::~CodeSyntaxHighlighter()
{
    m_rehighlightTimer.stop();
    m_visibleRangeTimer.stop();
    m_stateCacheTimer.stop();
    disconnect(m_contentsChangeConnection);
    setDocument(nullptr);
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

int CodeSyntaxHighlighter::firstVisibleBlock() const
{
    return m_firstVisibleBlock;
}

int CodeSyntaxHighlighter::lastVisibleBlock() const
{
    return m_lastVisibleBlock;
}

void CodeSyntaxHighlighter::setTextDocument(QQuickTextDocument *document)
{
    if (document == m_textDocument) {
        return;
    }
    disconnect(m_contentsChangeConnection);
    m_rehighlightTimer.stop();
    m_visibleRangeTimer.stop();
    m_stateCacheTimer.stop();
    m_pendingBlocks.clear();
    m_pendingBlockIndex = 0;
    m_activeFirstBlock = -1;
    m_activeLastBlock = -1;
    m_stateCacheDirty = true;
    m_stateCacheBuilding = false;
    m_blockStartStates.clear();
    m_formattedBlockGenerations.clear();
    m_highlightingEnabled = false;
    m_textDocument = document;
    setDocument(document ? document->textDocument() : nullptr);
    if (this->document()) {
        m_contentsChangeConnection = connect(
            this->document(), &QTextDocument::contentsChange,
            this, [this](int, int removed, int added) {
                if (m_processingHighlight || (removed == 0 && added == 0)) {
                    return;
                }
                invalidateFormattedBlocks();
                m_stateCacheDirty = true;
                scheduleRehighlight();
            });
    }
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
    updateDefinition();
    invalidateFormattedBlocks();
    m_stateCacheDirty = true;
    scheduleRehighlight();
    emit languageChanged();
}

void CodeSyntaxHighlighter::setDarkTheme(bool darkTheme)
{
    if (darkTheme == m_darkTheme) {
        return;
    }
    m_darkTheme = darkTheme;
    updateTheme();
    invalidateFormattedBlocks();
    scheduleRehighlight();
    emit darkThemeChanged();
}

void CodeSyntaxHighlighter::setFirstVisibleBlock(int blockNumber)
{
    if (blockNumber == m_firstVisibleBlock) {
        return;
    }
    m_firstVisibleBlock = blockNumber;
    m_visibleRangeTimer.start(0);
    emit visibleBlockRangeChanged();
}

void CodeSyntaxHighlighter::setLastVisibleBlock(int blockNumber)
{
    if (blockNumber == m_lastVisibleBlock) {
        return;
    }
    m_lastVisibleBlock = blockNumber;
    m_visibleRangeTimer.start(0);
    emit visibleBlockRangeChanged();
}

void CodeSyntaxHighlighter::highlightBlock(const QString &text)
{
    if (!m_highlightingEnabled || !document()) {
        return;
    }

    if (m_stateCacheDirty || m_stateCacheLanguage != m_language) {
        startBlockStateCacheRebuild();
        return;
    }
    if (m_stateCacheBuilding
        || m_blockStartStates.size() != document()->blockCount()) {
        return;
    }

    const int blockNumber = currentBlock().blockNumber();
    if (m_activeFirstBlock >= 0
        && (blockNumber < m_activeFirstBlock || blockNumber > m_activeLastBlock)) {
        return;
    }

    const KSyntaxHighlighting::State state =
        blockNumber >= 0 && blockNumber < m_blockStartStates.size()
        ? m_blockStartStates.at(blockNumber)
        : KSyntaxHighlighting::State();
    m_collectFormats = true;
    highlightLine(text, state);
    m_collectFormats = false;
}

void CodeSyntaxHighlighter::applyFormat(
    int offset,
    int length,
    const KSyntaxHighlighting::Format &format)
{
    if (!m_collectFormats || length <= 0 || !format.isValid()) {
        return;
    }

    const KSyntaxHighlighting::Theme currentTheme = theme();
    if (!currentTheme.isValid() || format.isDefaultTextStyle(currentTheme)) {
        return;
    }

    const int formatId = format.id();
    auto iterator = m_textFormats.constFind(formatId);
    if (iterator == m_textFormats.cend()) {
        QTextCharFormat textFormat;
        textFormat.setForeground(format.textColor(currentTheme));
        if (format.hasBackgroundColor(currentTheme)) {
            textFormat.setBackground(format.backgroundColor(currentTheme));
        }
        if (format.isBold(currentTheme)) {
            textFormat.setFontWeight(QFont::Bold);
        }
        if (format.isItalic(currentTheme)) {
            textFormat.setFontItalic(true);
        }
        if (format.isUnderline(currentTheme)) {
            textFormat.setFontUnderline(true);
        }
        if (format.isStrikeThrough(currentTheme)) {
            textFormat.setFontStrikeOut(true);
        }
        m_textFormats.insert(formatId, std::move(textFormat));
        iterator = m_textFormats.constFind(formatId);
    }
    setFormat(offset, length, iterator.value());
}

void CodeSyntaxHighlighter::invalidateFormattedBlocks()
{
    ++m_formatGeneration;
    if (m_formatGeneration == 0) {
        m_formattedBlockGenerations.clear();
        m_formatGeneration = 1;
    }
}

void CodeSyntaxHighlighter::scheduleRehighlight()
{
    if (!m_highlightingEnabled || !document()) {
        return;
    }
    if (m_processingHighlight) {
        QTimer::singleShot(0, this, &CodeSyntaxHighlighter::scheduleRehighlight);
        return;
    }
    m_rehighlightTimer.stop();
    if (m_stateCacheDirty || m_stateCacheLanguage != m_language) {
        startBlockStateCacheRebuild();
        return;
    }
    if (m_stateCacheBuilding) {
        return;
    }
    if (m_blockStartStates.size() != document()->blockCount()) {
        startBlockStateCacheRebuild();
        return;
    }

    const int blockCount = document()->blockCount();
    if (m_firstVisibleBlock >= 0 && m_lastVisibleBlock >= m_firstVisibleBlock) {
        // Format near the viewport, but retain blocks already visited. Clearing a
        // block above the viewport invalidates downstream text layout and makes
        // the next QQuickTextEdit cursor hit-test pay for that layout again.
        constexpr int kViewportBufferBlocks = 64;
        m_activeFirstBlock = qMax(0, m_firstVisibleBlock - kViewportBufferBlocks);
        m_activeLastBlock = qMin(blockCount - 1,
                                 m_lastVisibleBlock + kViewportBufferBlocks);
    } else {
        m_activeFirstBlock = -1;
        m_activeLastBlock = -1;
    }

    m_pendingBlocks.clear();
    m_pendingBlockIndex = 0;
    const int newFirstBlock = m_activeFirstBlock >= 0 ? m_activeFirstBlock : 0;
    const int newLastBlock = m_activeFirstBlock >= 0
        ? m_activeLastBlock
        : blockCount - 1;
    m_pendingBlocks.reserve(newLastBlock - newFirstBlock + 1);
    for (int blockNumber = newFirstBlock; blockNumber <= newLastBlock; ++blockNumber) {
        if (m_formattedBlockGenerations.value(blockNumber, 0)
            != m_formatGeneration) {
            m_pendingBlocks.append(blockNumber);
        }
    }
    m_rehighlightTimer.start(0);
}

void CodeSyntaxHighlighter::startBlockStateCacheRebuild()
{
    m_stateCacheTimer.stop();
    m_blockStartStates.clear();
    if (!document()) {
        m_stateCacheDirty = true;
        m_stateCacheBuilding = false;
        m_stateCacheLanguage.clear();
        return;
    }

    m_blockStartStates.reserve(document()->blockCount());
    m_stateCacheDirty = false;
    m_stateCacheBuilding = true;
    m_stateCacheLanguage = m_language;
    m_stateCacheNextBlock = document()->firstBlock();
    m_stateCacheNextState = KSyntaxHighlighting::State();
    m_stateCacheTimer.start(0);
}

void CodeSyntaxHighlighter::rebuildBlockStateCacheChunk()
{
    if (!m_stateCacheBuilding || !document()) {
        return;
    }

    // KSyntaxHighlighting state is cheap to retain and preserves multiline syntax
    // while only the blocks near the viewport receive QTextCharFormat ranges.
    m_collectFormats = false;
    QElapsedTimer timer;
    timer.start();
    do {
        m_blockStartStates.append(m_stateCacheNextState);
        const QString text = m_stateCacheNextBlock.text();
        m_stateCacheNextState = highlightLine(text, m_stateCacheNextState);
        m_stateCacheNextBlock = m_stateCacheNextBlock.next();
    } while (m_stateCacheNextBlock.isValid() && timer.elapsed() < 4);

    if (m_stateCacheNextBlock.isValid()) {
        m_stateCacheTimer.start(0);
        return;
    }

    m_stateCacheBuilding = false;
    scheduleRehighlight();
}

void CodeSyntaxHighlighter::rehighlightNextChunk()
{
    if (!m_highlightingEnabled || !document()
        || m_pendingBlockIndex >= m_pendingBlocks.size()) {
        m_pendingBlocks.clear();
        m_pendingBlockIndex = 0;
        return;
    }

    QElapsedTimer timer;
    timer.start();
    do {
        const int requestedBlockNumber = m_pendingBlocks.at(m_pendingBlockIndex);
        const QTextBlock block = document()->findBlockByNumber(requestedBlockNumber);
        ++m_pendingBlockIndex;
        if (block.isValid()) {
            const int blockNumber = block.blockNumber();
            if (m_activeFirstBlock >= 0
                && (blockNumber < m_activeFirstBlock
                    || blockNumber > m_activeLastBlock)) {
                continue;
            }
            m_processingHighlight = true;
            rehighlightBlock(block);
            m_processingHighlight = false;
            if (m_language.isEmpty()) {
                m_formattedBlockGenerations.remove(blockNumber);
            } else {
                m_formattedBlockGenerations.insert(blockNumber,
                                                   m_formatGeneration);
            }
        } else {
            m_formattedBlockGenerations.remove(requestedBlockNumber);
        }
    } while (m_pendingBlockIndex < m_pendingBlocks.size() && timer.elapsed() < 4);

    if (m_pendingBlockIndex < m_pendingBlocks.size()) {
        m_rehighlightTimer.start(0);
    } else {
        m_pendingBlocks.clear();
        m_pendingBlockIndex = 0;
    }
}

void CodeSyntaxHighlighter::updateDefinition()
{
    KSyntaxHighlighting::Definition definition;
    if (m_language == QStringLiteral("javascript")) {
        definition = syntaxRepository().definitionForName(QStringLiteral("JavaScript"));
    } else if (m_language == QStringLiteral("lua")) {
        definition = syntaxRepository().definitionForName(QStringLiteral("Lua"));
    }
    KSyntaxHighlighting::AbstractHighlighter::setDefinition(definition);
    m_textFormats.clear();
}

void CodeSyntaxHighlighter::updateTheme()
{
    const auto defaultTheme = m_darkTheme
        ? KSyntaxHighlighting::Repository::DarkTheme
        : KSyntaxHighlighting::Repository::LightTheme;
    KSyntaxHighlighting::AbstractHighlighter::setTheme(
        syntaxRepository().defaultTheme(defaultTheme));
    m_textFormats.clear();
}
