#pragma once

#include <QPointer>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextBlock>
#include <QTimer>
#include <QtQmlIntegration/qqmlintegration.h>

class CodeSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickTextDocument* textDocument READ textDocument WRITE setTextDocument NOTIFY textDocumentChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)

public:
    explicit CodeSyntaxHighlighter(QObject *parent = nullptr);

    QQuickTextDocument *textDocument() const;
    QString language() const;
    bool darkTheme() const;

    void setTextDocument(QQuickTextDocument *document);
    void setLanguage(const QString &language);
    void setDarkTheme(bool darkTheme);

signals:
    void textDocumentChanged();
    void languageChanged();
    void darkThemeChanged();

protected:
    void highlightBlock(const QString &text) override;

private:
    void rebuildFormats();
    void highlightJavaScript(const QString &text);
    void highlightLua(const QString &text);
    void formatIdentifier(const QString &text, int start, int length, bool javascript);
    void scheduleRehighlight();
    void rehighlightNextChunk();

    QPointer<QQuickTextDocument> m_textDocument;
    QString m_language;
    bool m_darkTheme = false;
    bool m_highlightingEnabled = true;
    QTextBlock m_nextBlock;
    QTimer m_rehighlightTimer;
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_builtinFormat;
};
