#include "models/processorlibrarymodel.h"
#include "services/processors/editorcodec.h"
#include "viewmodels/processorsviewmodel.h"
#include <QTemporaryDir>
#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <memory>

class ProcessorsViewModelTest : public QObject
{
    Q_OBJECT
private slots:
    void selectsWithoutExecutingCode();
    void filtersMetadataAndSource();
    void blocksDeletionWhileUsed();
    void rendersReadOnlyManagement();
};

static SaveProcessorRevisionResult seed(ProcessorLibrary &library, const QString &name, const QString &source)
{
    auto draft = EditorCodec::templateDraft("javascript");
    SaveProcessorRevisionCommand command;
    QString error;
    EditorCodec::command(draft, command, error);
    command.name = name;
    command.description = QStringLiteral("Synthetic vehicle frames");
    command.content.files[0].content = source.toUtf8();
    return library.saveRevision(command);
}

void ProcessorsViewModelTest::selectsWithoutExecutingCode()
{
    QTemporaryDir directory;
    ProcessorLibrary library(directory.path());
    QVERIFY(seed(library, "Demo", "while (true) {} function process(context) {}").ok);
    ProcessorLibraryModel model;
    QVector<SessionState> sessions;
    ProcessorsViewModel viewModel(library, model, sessions);
    viewModel.ensureEditorSelection();
    QVERIFY(!viewModel.currentProcessorId().isEmpty());
    QCOMPARE(model.rowAt(0).value("readinessState").toString(), QStringLiteral("not_validated"));
    QVERIFY(!viewModel.selectFilteredProcessorAt(-1));
    QVERIFY(viewModel.selectFilteredProcessorAt(0));
    QVERIFY(viewModel.metaObject()->indexOfMethod("saveEditor()") < 0);
    QVERIFY(viewModel.metaObject()->indexOfProperty("source") < 0);
}

void ProcessorsViewModelTest::filtersMetadataAndSource()
{
    QTemporaryDir directory;
    ProcessorLibrary library(directory.path());
    QVERIFY(seed(library, "Telemetry Formatter", "function process(context) { return context.decoded; }").ok);
    ProcessorLibraryModel model;
    QVector<SessionState> sessions;
    ProcessorsViewModel viewModel(library, model, sessions);
    for (const auto &query : {"vehicle", "JavaScript", "context.decoded"}) {
        viewModel.setProcessorFilterText(query);
        QCOMPARE(viewModel.filteredProcessors()->count(), 1);
    }
}

void ProcessorsViewModelTest::blocksDeletionWhileUsed()
{
    QTemporaryDir directory;
    ProcessorLibrary library(directory.path());
    const auto saved = seed(library, "Bound demo", "function process(context) { return context.decoded; }");
    QVERIFY(saved.ok);
    ProcessorLibraryModel model;
    QVector<SessionState> sessions(1);
    sessions[0].name = QStringLiteral("Demo session");
    SubscriptionEntry entry;
    entry.processor.processorId = saved.processor.id;
    sessions[0].subscriptions.append(entry);
    ProcessorsViewModel viewModel(library, model, sessions);
    viewModel.ensureEditorSelection();
    QVERIFY(!viewModel.deleteCurrent());
    QVERIFY(viewModel.diagnostics().contains(QStringLiteral("Demo session")));
    QVERIFY(library.processorById(saved.processor.id));
}

void ProcessorsViewModelTest::rendersReadOnlyManagement()
{
    QTemporaryDir directory;
    ProcessorLibrary library(directory.path());
    const auto saved = seed(library, "Demo telemetry decoder", "function process(context) { return context.decoded; }");
    QVERIFY(saved.ok);
    QVERIFY(seed(library, "Demo status formatter", "function process(context) { return context.topic; }").ok);
    ProcessorLibraryModel model;
    QVector<SessionState> sessions;
    ProcessorsViewModel viewModel(library, model, sessions);
    viewModel.ensureEditorSelection();
    model.setReadiness(saved.processor.id, QStringLiteral("ready"), {});
    QQmlEngine engine;
    QQmlComponent uiComponent(&engine);
    uiComponent.loadFromModule("MqttPlusProcessorViewTest", "AppUi");
    std::unique_ptr<QObject> ui(uiComponent.createWithInitialProperties({
        {QStringLiteral("isDarkTheme"), false}, {QStringLiteral("themeColor"), QStringLiteral("mint")},
        {QStringLiteral("animationsEnabled"), false}}));
    QVERIFY2(ui, qPrintable(uiComponent.errorString()));
    QQmlComponent component(&engine);
    component.loadFromModule("MqttPlusProcessorViewTest", "ProcessorsView");
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {QStringLiteral("ui"), QVariant::fromValue(ui.get())},
        {QStringLiteral("viewModel"), QVariant::fromValue(&viewModel)}}));
    QVERIFY2(page, qPrintable(component.errorString()));
    auto item = qobject_cast<QQuickItem *>(page.get());
    QVERIFY(item);
    QQuickWindow window;
    window.setColor(Qt::white);
    window.resize(1440, 900);
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(1440, 900));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(100);
    QVERIFY(!viewModel.currentProcessorId().isEmpty());
    for (auto *child : page->findChildren<QObject *>())
        QVERIFY(!QString::fromLatin1(child->metaObject()->className()).startsWith(QStringLiteral("QQuickTextEdit")));
    const auto capture = qEnvironmentVariable("MQTT_PLUS_TEST_CAPTURE");
    if (!capture.isEmpty()) {
        const auto image = window.grabWindow();
        QVERIFY(!image.isNull());
        QVERIFY(image.save(capture));
    }
    item->setParentItem(nullptr);
}

QTEST_MAIN(ProcessorsViewModelTest)
#include "test_processorsviewmodel.moc"
