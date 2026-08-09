#pragma once

#include <KSyntaxHighlighting/AbstractHighlighter>
#include <KSyntaxHighlighting/State>

#include <QHash>
#include <QPointer>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTimer>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

namespace KSyntaxHighlighting {
class Format;
}

class CodeSyntaxHighlighter : public QSyntaxHighlighter,
                              public KSyntaxHighlighting::AbstractHighlighter
{
    Q_OBJECT
    Q_INTERFACES(KSyntaxHighlighting::AbstractHighlighter)
    QML_ELEMENT
    Q_PROPERTY(QQuickTextDocument* textDocument READ textDocument WRITE setTextDocument NOTIFY textDocumentChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)
    Q_PROPERTY(int firstVisibleBlock READ firstVisibleBlock WRITE setFirstVisibleBlock NOTIFY visibleBlockRangeChanged)
    Q_PROPERTY(int lastVisibleBlock READ lastVisibleBlock WRITE setLastVisibleBlock NOTIFY visibleBlockRangeChanged)

public:
    explicit CodeSyntaxHighlighter(QObject *parent = nullptr);
    ~CodeSyntaxHighlighter() override;

    QQuickTextDocument *textDocument() const;
    QString language() const;
    bool darkTheme() const;
    int firstVisibleBlock() const;
    int lastVisibleBlock() const;

    void setTextDocument(QQuickTextDocument *document);
    void setLanguage(const QString &language);
    void setDarkTheme(bool darkTheme);
    void setFirstVisibleBlock(int blockNumber);
    void setLastVisibleBlock(int blockNumber);

signals:
    void textDocumentChanged();
    void languageChanged();
    void darkThemeChanged();
    void visibleBlockRangeChanged();

protected:
    void highlightBlock(const QString &text) override;

private:
    void applyFormat(int offset, int length,
                     const KSyntaxHighlighting::Format &format) override;
    void invalidateFormattedBlocks();
    void rebuildBlockStateCacheChunk();
    void scheduleRehighlight();
    void startBlockStateCacheRebuild();
    void rehighlightNextChunk();
    void updateDefinition();
    void updateTheme();

    QPointer<QQuickTextDocument> m_textDocument;
    QString m_language;
    bool m_darkTheme = false;
    bool m_highlightingEnabled = true;
    bool m_processingHighlight = false;
    bool m_collectFormats = false;
    int m_firstVisibleBlock = -1;
    int m_lastVisibleBlock = -1;
    int m_activeFirstBlock = -1;
    int m_activeLastBlock = -1;
    bool m_stateCacheDirty = true;
    bool m_stateCacheBuilding = false;
    QString m_stateCacheLanguage;
    QTextBlock m_stateCacheNextBlock;
    KSyntaxHighlighting::State m_stateCacheNextState;
    QVector<KSyntaxHighlighting::State> m_blockStartStates;
    QVector<int> m_pendingBlocks;
    QHash<int, quint64> m_formattedBlockGenerations;
    QHash<int, QTextCharFormat> m_textFormats;
    quint64 m_formatGeneration = 1;
    qsizetype m_pendingBlockIndex = 0;
    QMetaObject::Connection m_contentsChangeConnection;
    QTimer m_visibleRangeTimer;
    QTimer m_stateCacheTimer;
    QTimer m_rehighlightTimer;
};
