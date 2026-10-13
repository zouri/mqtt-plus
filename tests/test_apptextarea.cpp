#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

class AppTextAreaTest : public QObject
{
    Q_OBJECT
private slots:
    void preservesTextAndOptionalLineNumbers();
    void insertsRemovesAndScrollsPlainText();
};

static std::unique_ptr<QObject> createUi(QQmlEngine &engine)
{
    QQmlComponent component(&engine);
    component.loadFromModule("MqttPlusTextAreaTest", "AppUi");
    return std::unique_ptr<QObject>(component.createWithInitialProperties({
        {QStringLiteral("isDarkTheme"), false}, {QStringLiteral("themeColor"), QStringLiteral("mint")},
        {QStringLiteral("animationsEnabled"), false}}));
}

void AppTextAreaTest::preservesTextAndOptionalLineNumbers()
{
    QQmlEngine engine;
    auto ui = createUi(engine);
    QVERIFY(ui);
    QQmlComponent component(&engine);
    component.loadFromModule("MqttPlusTextAreaTest", "AppTextArea");
    std::unique_ptr<QObject> editor(component.createWithInitialProperties({
        {QStringLiteral("ui"), QVariant::fromValue(ui.get())},
        {QStringLiteral("showLineNumbers"), true},
        {QStringLiteral("text"), QStringLiteral("first\nsecond\nthird")}}));
    QVERIFY2(editor, qPrintable(component.errorString()));
    QTRY_COMPARE(editor->property("lineCount").toInt(), 3);
    QCOMPARE(editor->property("text").toString(), QStringLiteral("first\nsecond\nthird"));
    QVERIFY(editor->metaObject()->indexOfProperty("syntaxLanguage") < 0);
    editor->setProperty("showLineNumbers", false);
    QCOMPARE(editor->property("lineNumberGutterWidth").toInt(), 0);
    editor->setProperty("showLineNumbers", true);
    QVERIFY(editor->property("lineNumberGutterWidth").toInt() > 0);
    QCOMPARE(editor->property("text").toString(), QStringLiteral("first\nsecond\nthird"));
    editor->setProperty("readOnly", true);
    QVERIFY(editor->property("readOnly").toBool());
}

void AppTextAreaTest::insertsRemovesAndScrollsPlainText()
{
    QQmlEngine engine;
    auto ui = createUi(engine);
    QVERIFY(ui);
    QQmlComponent component(&engine);
    component.loadFromModule("MqttPlusTextAreaTest", "AppTextArea");
    std::unique_ptr<QObject> editor(component.createWithInitialProperties({
        {QStringLiteral("ui"), QVariant::fromValue(ui.get())},
        {QStringLiteral("showLineNumbers"), true}, {QStringLiteral("text"), QStringLiteral("body")}}));
    QVERIFY2(editor, qPrintable(component.errorString()));
    auto item = qobject_cast<QQuickItem *>(editor.get());
    QVERIFY(item);
    QQuickWindow window;
    window.resize(500, 250);
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(500, 250));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(QMetaObject::invokeMethod(editor.get(), "insertText",
        Q_ARG(QVariant, QVariant(0)), Q_ARG(QVariant, QVariant(QStringLiteral("prefix\n")))));
    QCOMPARE(editor->property("text").toString(), QStringLiteral("prefix\nbody"));
    QVERIFY(QMetaObject::invokeMethod(editor.get(), "removeText",
        Q_ARG(QVariant, QVariant(0)), Q_ARG(QVariant, QVariant(7))));
    QCOMPARE(editor->property("text").toString(), QStringLiteral("body"));
    editor->setProperty("text", QStringLiteral("demo line\n").repeated(100));
    QTRY_VERIFY(editor->property("contentHeight").toReal() > editor->property("viewportHeight").toReal());
    QVERIFY(QMetaObject::invokeMethod(editor.get(), "setContentY", Q_ARG(QVariant, QVariant(100000))));
    QTRY_VERIFY(editor->property("contentY").toReal() > 0);
    QVERIFY(QMetaObject::invokeMethod(editor.get(), "setContentY", Q_ARG(QVariant, QVariant(-1))));
    QTRY_COMPARE(editor->property("contentY").toReal(), 0.0);
    item->setParentItem(nullptr);
}

QTEST_MAIN(AppTextAreaTest)
#include "test_apptextarea.moc"
