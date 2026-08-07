#include "presentation/codesyntaxhighlighter.h"

#include <QtTest/QtTest>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

namespace {

QTextCharFormat formatAt(const QTextDocument &document, int position)
{
    const QTextBlock block = document.findBlock(position);
    const int positionInBlock = position - block.position();
    for (const QTextLayout::FormatRange &range : block.layout()->formats()) {
        if (positionInBlock >= range.start
            && positionInBlock < range.start + range.length) {
            return range.format;
        }
    }
    return {};
}

QColor colorAt(const QTextDocument &document, int position)
{
    return formatAt(document, position).foreground().color();
}

} // namespace

class CodeSyntaxHighlighterTest : public QObject
{
    Q_OBJECT

private slots:
    void highlightsLanguageSyntax_data();
    void highlightsLanguageSyntax();
    void carriesMultilineCommentsAcrossBlocks_data();
    void carriesMultilineCommentsAcrossBlocks();
    void updatesColorsForDarkTheme();
    void parsesLargeDocumentWithinBudget_data();
    void parsesLargeDocumentWithinBudget();
    void focusesLargeHighlightedEditorPromptly();
};

void CodeSyntaxHighlighterTest::highlightsLanguageSyntax_data()
{
    QTest::addColumn<QString>("language");
    QTest::addColumn<QString>("source");
    QTest::addColumn<QString>("keyword");
    QTest::addColumn<QString>("stringToken");
    QTest::addColumn<QString>("numberToken");
    QTest::addColumn<QString>("commentToken");

    QTest::newRow("javascript")
        << QStringLiteral("javascript")
        << QStringLiteral("const label = \"消息\"; return parse(42); // 注释")
        << QStringLiteral("const")
        << QStringLiteral("\"消息\"")
        << QStringLiteral("42")
        << QStringLiteral("// 注释");
    QTest::newRow("lua")
        << QStringLiteral("lua")
        << QStringLiteral("local label = \"消息\"; return parse(42) -- 注释")
        << QStringLiteral("local")
        << QStringLiteral("\"消息\"")
        << QStringLiteral("42")
        << QStringLiteral("-- 注释");
}

void CodeSyntaxHighlighterTest::highlightsLanguageSyntax()
{
    QFETCH(QString, language);
    QFETCH(QString, source);
    QFETCH(QString, keyword);
    QFETCH(QString, stringToken);
    QFETCH(QString, numberToken);
    QFETCH(QString, commentToken);

    QTextDocument document(source);
    CodeSyntaxHighlighter highlighter;
    highlighter.setDocument(&document);
    highlighter.setLanguage(language);
    highlighter.rehighlight();

    const QColor keywordColor = colorAt(document, source.indexOf(keyword));
    const QColor stringColor = colorAt(document, source.indexOf(stringToken));
    const QColor numberColor = colorAt(document, source.indexOf(numberToken));
    const QColor commentColor = colorAt(document, source.indexOf(commentToken));
    const QColor functionColor = colorAt(document, source.indexOf(QStringLiteral("parse")));

    QVERIFY(keywordColor.isValid());
    QVERIFY(stringColor.isValid());
    QVERIFY(numberColor.isValid());
    QVERIFY(commentColor.isValid());
    QVERIFY(functionColor.isValid());
    QVERIFY(keywordColor != stringColor);
    QVERIFY(stringColor != numberColor);
    QVERIFY(commentColor != functionColor);
    QVERIFY(formatAt(document, source.indexOf(commentToken)).fontItalic());
}

void CodeSyntaxHighlighterTest::carriesMultilineCommentsAcrossBlocks_data()
{
    QTest::addColumn<QString>("language");
    QTest::addColumn<QString>("source");
    QTest::addColumn<QString>("commentToken");
    QTest::addColumn<QString>("keyword");

    QTest::newRow("javascript")
        << QStringLiteral("javascript")
        << QStringLiteral("/* first\nsecond */ const value = 1;")
        << QStringLiteral("second")
        << QStringLiteral("const");
    QTest::newRow("lua")
        << QStringLiteral("lua")
        << QStringLiteral("--[[ first\nsecond ]] local value = 1")
        << QStringLiteral("second")
        << QStringLiteral("local");
}

void CodeSyntaxHighlighterTest::carriesMultilineCommentsAcrossBlocks()
{
    QFETCH(QString, language);
    QFETCH(QString, source);
    QFETCH(QString, commentToken);
    QFETCH(QString, keyword);

    QTextDocument document(source);
    CodeSyntaxHighlighter highlighter;
    highlighter.setDocument(&document);
    highlighter.setLanguage(language);
    highlighter.rehighlight();

    const QTextCharFormat commentFormat = formatAt(document, source.indexOf(commentToken));
    const QTextCharFormat keywordFormat = formatAt(document, source.indexOf(keyword));
    QVERIFY(commentFormat.fontItalic());
    QVERIFY(keywordFormat.fontWeight() >= QFont::DemiBold);
    QVERIFY(commentFormat.foreground().color() != keywordFormat.foreground().color());
}

void CodeSyntaxHighlighterTest::updatesColorsForDarkTheme()
{
    const QString source = QStringLiteral("const value = \"text\";");
    QTextDocument document(source);
    CodeSyntaxHighlighter highlighter;
    highlighter.setDocument(&document);
    highlighter.setLanguage(QStringLiteral("javascript"));
    highlighter.rehighlight();
    const QColor lightKeyword = colorAt(document, 0);

    highlighter.setDarkTheme(true);
    highlighter.rehighlight();
    const QColor darkKeyword = colorAt(document, 0);

    QVERIFY(lightKeyword.isValid());
    QVERIFY(darkKeyword.isValid());
    QVERIFY(lightKeyword != darkKeyword);
}

void CodeSyntaxHighlighterTest::parsesLargeDocumentWithinBudget_data()
{
    QTest::addColumn<QString>("language");
    QTest::addColumn<QString>("line");

    QTest::newRow("javascript")
        << QStringLiteral("javascript")
        << QStringLiteral("const value = parse(context.payload, 42); // message field\n");
    QTest::newRow("lua")
        << QStringLiteral("lua")
        << QStringLiteral("local value = parse(context.payload, 42) -- message field\n");
}

void CodeSyntaxHighlighterTest::parsesLargeDocumentWithinBudget()
{
    QFETCH(QString, language);
    QFETCH(QString, line);

    QString source;
    source.reserve(line.size() * 4000);
    for (int index = 0; index < 4000; ++index) {
        source += line;
    }

    QTextDocument document(source);
    CodeSyntaxHighlighter highlighter;
    highlighter.setDocument(&document);
    highlighter.setLanguage(language);

    QElapsedTimer timer;
    timer.start();
    highlighter.rehighlight();
    const qint64 elapsedMilliseconds = timer.elapsed();

    QVERIFY2(elapsedMilliseconds < 100,
             qPrintable(QStringLiteral("Highlighting blocked for %1 ms")
                            .arg(elapsedMilliseconds)));
}

void CodeSyntaxHighlighterTest::focusesLargeHighlightedEditorPromptly()
{
    if (QGuiApplication::screens().isEmpty()) {
        QSKIP("A screen is required for the editor interaction test");
    }

    qmlRegisterType<CodeSyntaxHighlighter>("CodeEditorTest", 1, 0,
                                           "CodeSyntaxHighlighter");

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import QtQuick.Controls.Basic
        import CodeEditorTest

        Window {
            id: root
            width: 800
            height: 600
            property string source: ""
            property string syntaxLanguage: ""
            readonly property int lineCount: Math.max(1, editor.lineCount)

            Item {
                anchors.fill: parent

                ScrollView {
                    id: scroll
                    anchors.fill: parent
                    anchors.leftMargin: 58
                    clip: true

                    TextArea {
                        id: editor
                        objectName: "editor"
                        width: scroll.availableWidth
                        text: root.source
                        wrapMode: TextEdit.NoWrap
                        font.family: "Menlo"
                        font.pixelSize: 13
                        selectByMouse: true
                        onTextChanged: {
                            if (text !== root.source)
                                root.source = text
                        }
                    }
                }

                ListView {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 58
                    interactive: false
                    clip: true
                    contentY: scroll.contentItem.contentY
                    model: root.lineCount

                    delegate: Label {
                        required property int index
                        width: ListView.view.width
                        height: Math.max(1, editor.contentHeight / root.lineCount)
                        text: index + 1
                    }
                }
            }

            CodeSyntaxHighlighter {
                objectName: "highlighter"
                textDocument: editor.textDocument
                language: root.syntaxLanguage
            }
        }
    )", QUrl(QStringLiteral("qrc:/CodeEditorPerformance.qml")));

    QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 1000);
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> root(component.create());
    QVERIFY2(root, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(root.get());
    QVERIFY(window);
    auto *editor = window->findChild<QQuickItem *>(QStringLiteral("editor"));
    QVERIFY(editor);

    const QString line = QStringLiteral(
        "local value = parse(context.payload, 42) -- message field\n");
    QString source;
    source.reserve(line.size() * 4000);
    for (int index = 0; index < 4000; ++index) {
        source += line;
    }
    root->setProperty("source", source);
    QElapsedTimer highlightTimer;
    highlightTimer.start();
    root->setProperty("syntaxLanguage", QStringLiteral("lua"));
    const qint64 highlightMilliseconds = highlightTimer.elapsed();
    QVERIFY2(highlightMilliseconds < 100,
             qPrintable(QStringLiteral("Starting highlighting blocked for %1 ms")
                            .arg(highlightMilliseconds)));

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QCoreApplication::processEvents();
    editor->setFocus(false);

    QElapsedTimer timer;
    timer.start();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(300, 200));
    const qint64 elapsedMilliseconds = timer.elapsed();

    QVERIFY(editor->hasActiveFocus());
    QVERIFY2(elapsedMilliseconds < 100,
             qPrintable(QStringLiteral("Click-to-focus blocked for %1 ms")
                            .arg(elapsedMilliseconds)));

    auto *highlighter = window->findChild<CodeSyntaxHighlighter *>(
        QStringLiteral("highlighter"));
    QVERIFY(highlighter);
    const int lastKeywordPosition = source.lastIndexOf(QStringLiteral("local"));
    QTRY_VERIFY_WITH_TIMEOUT(
        colorAt(*highlighter->document(), lastKeywordPosition).isValid(), 5000);
}

QTEST_MAIN(CodeSyntaxHighlighterTest)

#include "test_codesyntaxhighlighter.moc"
