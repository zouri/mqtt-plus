#include "usecases/browsereditorservice.h"
#include "services/processors/editorcodec.h"
#include "usecases/editorwindowlease.h"
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <QDirIterator>
#include <QCborValue>
#include <QProcess>
#include <QProcessEnvironment>
#include <QThread>
#include <QScopeGuard>
#include <QtEndian>
#include <QtTest>
#include <cstdio>
#include <cstring>

class BrowserEditorServiceTest : public QObject
{
    Q_OBJECT
private slots:
    void init() { m_window.clear(); m_leaseToken.clear(); }
    void authenticatesValidatesAndSaves();
    void rejectsInvalidOriginsAndHosts();
    void containsValidatorFailuresAndCancellation();
    void cachesStaticAssetsWithoutCachingEditorData();
    void permitsOnlyOneEditableWindow();
    void expiresAndFencesWindowLeases();
    void rejectsSaveAfterWindowRelease();
private:
    QString m_window;
    QString m_leaseToken;
    struct Result { int status; QJsonObject body; };
    Result request(const QString &origin, const QString &path, const QString &method,
        const QJsonObject &body = {}, const QString &token = {}, const QByteArray &host = {},
        const QString &window = {}, const QString &leaseToken = {});
};

BrowserEditorServiceTest::Result BrowserEditorServiceTest::request(const QString &origin,
    const QString &path, const QString &method, const QJsonObject &body, const QString &token, const QByteArray &host,
    const QString &window, const QString &leaseToken)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(origin + "/api/v1" + path));
    request.setRawHeader("Origin", origin.toUtf8());
    request.setRawHeader("Content-Type", "application/json");
    if (!host.isEmpty()) request.setRawHeader("Host", host);
    if (!token.isEmpty()) request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    request.setRawHeader("X-Editor-Window", (window.isEmpty() ? m_window : window).toUtf8());
    request.setRawHeader("X-Editor-Lease", (leaseToken.isEmpty() ? m_leaseToken : leaseToken).toUtf8());
    QNetworkReply *reply = method == "GET" ? manager.get(request)
        : manager.sendCustomRequest(request, method.toUtf8(), QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(10000);
    loop.exec();
    return {reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), QJsonDocument::fromJson(reply->readAll()).object()};
}

void BrowserEditorServiceTest::cachesStaticAssetsWithoutCachingEditorData()
{
    QTemporaryDir directory;
    ProcessorLibrary snapshot(directory.path(), true);
    BrowserEditorService service(snapshot);
    QSignalSpy opening(&service, &BrowserEditorService::openingLink);
    service.openEditor({}, "lua", false);
    QTRY_COMPARE(opening.size(), 1);
    QUrl url(opening.first().first().toString());
    url.setFragment({});
    QNetworkAccessManager manager;
    const auto cacheHeader = [&manager, &url](const QString &path) {
        auto requestUrl = url;
        requestUrl.setPath(path);
        auto reply = manager.get(QNetworkRequest(requestUrl));
        QSignalSpy finished(reply, &QNetworkReply::finished);
        if (!finished.wait(5000)) return QByteArray();
        const auto header = reply->rawHeader("Cache-Control");
        reply->deleteLater();
        return header;
    };
    QString fontPath;
    QDirIterator assets(QStringLiteral(":/processor-editor/assets"), QDir::Files);
    while (assets.hasNext()) {
        const auto file = assets.next();
        if (file.endsWith(QStringLiteral(".ttf"))) fontPath = file.sliced(QStringLiteral(":/processor-editor").size());
    }
    QVERIFY(!fontPath.isEmpty());
    QCOMPARE(cacheHeader("/"), QByteArray("no-store"));
    QCOMPARE(cacheHeader(fontPath), QByteArray("public, max-age=31536000, immutable"));
    QCOMPARE(cacheHeader("/api/v1/bootstrap"), QByteArray("no-store"));
    QCOMPARE(cacheHeader("/assets/missing.js"), QByteArray("no-store"));
}

void BrowserEditorServiceTest::authenticatesValidatesAndSaves()
{
    QTemporaryDir directory;
    ProcessorLibrary snapshot(directory.path(), true);
    BrowserEditorService service(snapshot);
    QSignalSpy opening(&service, &BrowserEditorService::openingLink);
    service.openEditor({}, "lua", false);
    QTRY_COMPARE(opening.size(), 1);
    QUrl url(opening.takeFirst().first().toString());
    const auto ticket = QUrlQuery(url.fragment()).queryItemValue("ticket");
    url.setFragment({}); url.setPath({});
    const auto origin = url.toString();
    QCOMPARE(request(origin, "/bootstrap", "GET").status, 401);
    const auto exchange = request(origin, "/sessions/exchange", "POST", {{"ticket", ticket}});
    QCOMPARE(exchange.status, 200);
    const auto token = exchange.body.value("token").toString();
    m_window = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_leaseToken = request(origin, "/editor/lease", "POST", {{"windowId", m_window}}, token).body.value("leaseToken").toString();
    QCOMPARE(request(origin, "/sessions/exchange", "POST", {{"ticket", ticket}}).status, 401);
    QCOMPARE(request(origin, "/bootstrap", "GET", {}, token).status, 200);
    const auto created = request(origin, "/drafts", "POST", {{"languageId", "lua"}}, token);
    QCOMPARE(created.status, 201);
    const auto id = created.body.value("draftId").toString();
    const auto submitted = request(origin, "/drafts/" + id + "/save", "POST", {{"draftVersion", "1"}, {"requestId", "http-save"}}, token);
    QCOMPARE(submitted.status, 202);
    const auto job = submitted.body.value("jobId").toString();
    QJsonObject result;
    QTRY_VERIFY_WITH_TIMEOUT((result = request(origin, "/jobs/" + job, "GET", {}, token).body).value("state") == "completed", 10000);
    QCOMPARE(result.value("status").toInt(), 200);
    QCOMPARE(snapshot.processors().size(), 1);
    QCOMPARE(request(origin, "/requests/http-save", "GET", {}, token).status, 200);
    auto invalid = EditorCodec::templateDraft("javascript");
    auto files = invalid.value("files").toArray();
    auto file = files.first().toObject(); file.insert("content", "while (true) {}\nfunction process(context) { return context.decoded; }"); files[0] = file; invalid.insert("files", files);
    const auto bad = request(origin, "/drafts", "POST", invalid, token);
    QCOMPARE(bad.status, 201);
    const auto validation = request(origin, "/drafts/" + bad.body.value("draftId").toString() + "/validate", "POST", {{"draftVersion", "1"}}, token);
    QCOMPARE(validation.status, 202);
    QTRY_VERIFY_WITH_TIMEOUT((result = request(origin, "/jobs/" + validation.body.value("jobId").toString(), "GET", {}, token).body).value("state") == "completed", 10000);
    QCOMPARE(result.value("status").toInt(), 422);
    QCOMPARE(snapshot.processors().size(), 1);
}

void BrowserEditorServiceTest::rejectsInvalidOriginsAndHosts()
{
    QTemporaryDir directory;
    ProcessorLibrary snapshot(directory.path(), true);
    BrowserEditorService service(snapshot);
    QSignalSpy opening(&service, &BrowserEditorService::openingLink);
    service.openEditor({}, "lua", false);
    QTRY_COMPARE(opening.size(), 1);
    QUrl url(opening.first().first().toString()); url.setFragment({}); url.setPath({});
    QCOMPARE(request(url.toString(), "/bootstrap", "GET", {}, {}, "malicious.example").status, 403);
    QNetworkAccessManager manager;
    QNetworkRequest raw(QUrl(url.toString() + "/api/v1/sessions/exchange"));
    raw.setRawHeader("Origin", "https://malicious.example");
    raw.setRawHeader("Content-Type", "application/json");
    auto reply = manager.post(raw, "{}");
    QSignalSpy finished(reply, &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 403);
}

static int verifyPackagedValidator(const QString &program)
{
    auto environment = QProcessEnvironment::systemEnvironment();
    for (const auto &key : {"DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH", "QT_PLUGIN_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH"})
        environment.remove(QString::fromLatin1(key));
    environment.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin:/bin:/usr/sbin:/sbin"));
    for (const auto &language : {"lua", "javascript"}) {
        for (const bool valid : {true, false}) {
            auto draft = EditorCodec::templateDraft(QString::fromLatin1(language));
            if (!valid) {
                auto files = draft.value("files").toArray();
                auto file = files.first().toObject();
                file.insert("content", "function process( !!!");
                files[0] = file;
                draft.insert("files", files);
            }
            const auto content = QCborValue::fromJsonValue(draft).toCbor();
            QByteArray frame(4, Qt::Uninitialized);
            qToBigEndian(quint32(content.size()), frame.data());
            QProcess process;
            process.setProcessEnvironment(environment);
            process.start(program, {QStringLiteral("--processor-validator")});
            if (!process.waitForStarted(5000)) return 1;
            process.write(frame + content);
            process.closeWriteChannel();
            if (!process.waitForFinished(5000)) { process.kill(); process.waitForFinished(); return 1; }
            const auto output = process.readAllStandardOutput();
            if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0
                || output.size() < 4 || qFromBigEndian<quint32>(output.constData()) != output.size() - 4) return 1;
            QCborParserError error;
            const auto result = QCborValue::fromCbor(output.sliced(4), &error).toJsonValue().toObject();
            if (error.error != QCborError::NoError || !result.value("ok").isBool()
                || result.value("ok").toBool() != valid || !result.value("diagnostics").isArray()) return 1;
            std::printf("Packaged %s validator: %s input passed\n", language, valid ? "valid" : "invalid");
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--processor-validator") == 0) {
        const auto mode = qgetenv("MQTT_PLUS_TEST_VALIDATOR_MODE");
        if (mode == "crash") return 3;
        if (mode == "malformed") { std::fwrite("bad frame", 1, 9, stdout); return 0; }
        if (mode == "hang") { QCoreApplication app(argc, argv); return app.exec(); }
        if (mode == "delayed") QThread::msleep(300);
        return EditorCodec::runValidator(argc, argv);
    }
    QCoreApplication app(argc, argv);
    if (argc == 3 && std::strcmp(argv[1], "--verify-packaged-validator") == 0)
        return verifyPackagedValidator(QString::fromLocal8Bit(argv[2]));
    if (argc == 2 && (std::strcmp(argv[1], "--serve-editor") == 0 || std::strcmp(argv[1], "--serve-editor-pair") == 0)) {
        QTemporaryDir directory;
        ProcessorLibrary snapshot(directory.path(), true);
        BrowserEditorService service(snapshot);
        QObject::connect(&service, &BrowserEditorService::openingLink, &app, [](const QString &url) {
            std::printf("%s\n", qPrintable(url)); std::fflush(stdout);
        });
        service.openEditor({}, "lua", false);
        if (std::strcmp(argv[1], "--serve-editor-pair") == 0) service.openEditor({}, "javascript", false);
        return app.exec();
    }
    BrowserEditorServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

void BrowserEditorServiceTest::containsValidatorFailuresAndCancellation()
{
    QTemporaryDir directory;
    ProcessorLibrary snapshot(directory.path(), true);
    BrowserEditorService service(snapshot);
    QSignalSpy opening(&service, &BrowserEditorService::openingLink);
    service.openEditor({}, "lua", false);
    QTRY_COMPARE(opening.size(), 1);
    QUrl url(opening.first().first().toString());
    const auto ticket = QUrlQuery(url.fragment()).queryItemValue("ticket");
    url.setFragment({}); url.setPath({});
    const auto origin = url.toString();
    const auto token = request(origin, "/sessions/exchange", "POST", {{"ticket", ticket}}).body.value("token").toString();
    m_window = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_leaseToken = request(origin, "/editor/lease", "POST", {{"windowId", m_window}}, token).body.value("leaseToken").toString();
    const auto created = request(origin, "/drafts", "POST", {{"languageId", "lua"}}, token);
    QCOMPARE(created.status, 201);
    const auto id = created.body.value("draftId").toString();
    for (const auto &mode : {"crash", "malformed", "hang"}) {
        qputenv("MQTT_PLUS_TEST_VALIDATOR_MODE", mode);
        const auto submitted = request(origin, "/drafts/" + id + "/validate", "POST", {{"draftVersion", "1"}}, token);
        QCOMPARE(submitted.status, 202);
        QJsonObject result;
        const auto job = submitted.body.value("jobId").toString();
        QTRY_VERIFY_WITH_TIMEOUT((result = request(origin, "/jobs/" + job, "GET", {}, token).body).value("state") == "completed", 10000);
        QCOMPARE(result.value("status").toInt(), 422);
    }
    qputenv("MQTT_PLUS_TEST_VALIDATOR_MODE", "hang");
    const auto submitted = request(origin, "/drafts/" + id + "/save", "POST", {{"draftVersion", "1"}, {"requestId", "cancelled-save"}}, token);
    QCOMPARE(submitted.status, 202);
    const auto job = submitted.body.value("jobId").toString();
    QCOMPARE(request(origin, "/jobs/" + job, "DELETE", {}, token).status, 200);
    QTRY_COMPARE(request(origin, "/jobs/" + job, "GET", {}, token).body.value("state").toString(), QStringLiteral("cancelled"));
    QVERIFY(snapshot.processors().isEmpty());
    qunsetenv("MQTT_PLUS_TEST_VALIDATOR_MODE");
}

void BrowserEditorServiceTest::permitsOnlyOneEditableWindow()
{
    QTemporaryDir directory;
    ProcessorLibrary snapshot(directory.path(), true);
    BrowserEditorService service(snapshot);
    QSignalSpy opening(&service, &BrowserEditorService::openingLink);
    service.openEditor({}, "lua", false);
    QTRY_COMPARE(opening.size(), 1);
    QUrl url(opening.takeFirst().first().toString());
    const auto ticket = QUrlQuery(url.fragment()).queryItemValue("ticket");
    url.setFragment({}); url.setPath({});
    const auto origin = url.toString();
    const auto token = request(origin, "/sessions/exchange", "POST", {{"ticket", ticket}}).body.value("token").toString();
    QCOMPARE(request(origin, "/drafts", "POST", {{"languageId", "lua"}}, token).status, 423);
    m_window = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto first = request(origin, "/editor/lease", "POST", {{"windowId", m_window}}, token);
    QVERIFY(first.body.value("editable").toBool());
    m_leaseToken = first.body.value("leaseToken").toString();
    QVERIFY(!m_leaseToken.isEmpty());
    const auto otherWindow = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto other = request(origin, "/editor/lease", "POST", {{"windowId", otherWindow}}, token);
    QVERIFY(!other.body.value("editable").toBool());
    QVERIFY(!other.body.contains("leaseToken"));
    QCOMPARE(request(origin, "/drafts", "POST", {{"languageId", "javascript"}}, token, {}, otherWindow, "invalid").status, 423);
    QCOMPARE(request(origin, "/drafts", "GET", {}, token, {}, otherWindow, "invalid").status, 200);
    QCOMPARE(request(origin, "/editor/lease", "DELETE", {}, token, {}, otherWindow, "invalid").status, 423);
    const auto created = request(origin, "/drafts", "POST", {{"languageId", "lua"}}, token);
    QCOMPARE(created.status, 201);
    const auto id = created.body.value("draftId").toString();
    QCOMPARE(request(origin, "/drafts/" + id, "PUT", created.body, token, {}, otherWindow, "invalid").status, 423);
    QCOMPARE(request(origin, "/drafts/" + id, "DELETE", {}, token, {}, otherWindow, "invalid").status, 423);
    QCOMPARE(request(origin, "/drafts/" + id + "/save", "POST", {{"draftVersion", "1"}, {"requestId", "blocked"}}, token, {}, otherWindow, "invalid").status, 423);
    service.openEditor({}, "javascript", false);
    QTRY_COMPARE(opening.size(), 1);
    const QUrl secondUrl(opening.takeFirst().first().toString());
    const auto secondToken = request(origin, "/sessions/exchange", "POST", {{"ticket", QUrlQuery(secondUrl.fragment()).queryItemValue("ticket")}}).body.value("token").toString();
    QVERIFY(!request(origin, "/editor/lease", "POST", {{"windowId", otherWindow}}, secondToken).body.value("editable").toBool());
    QCOMPARE(request(origin, "/editor/lease", "DELETE", {}, token).status, 200);
    const auto acquired = request(origin, "/editor/lease", "POST", {{"windowId", otherWindow}}, secondToken);
    QVERIFY(acquired.body.value("editable").toBool());
    const auto secondLease = acquired.body.value("leaseToken").toString();
    QCOMPARE(request(origin, "/drafts", "POST", {{"languageId", "javascript"}}, secondToken, {}, otherWindow, secondLease).status, 201);
    QCOMPARE(request(origin, "/drafts", "POST", {{"languageId", "lua"}}, token).status, 423);
}

void BrowserEditorServiceTest::expiresAndFencesWindowLeases()
{
    qint64 now = 1000;
    EditorWindowLease lease([&now]() { return now; });
    const auto first = lease.acquire("session", "first").value("leaseToken").toString();
    QVERIFY(lease.allows("session", "first", first));
    now += 20000;
    QCOMPARE(lease.acquire("session", "first").value("leaseToken").toString(), first);
    now += 20000;
    QVERIFY(!lease.acquire("session", "second").value("editable").toBool());
    now += 10001;
    const auto second = lease.acquire("session", "second").value("leaseToken").toString();
    QVERIFY(!second.isEmpty());
    QVERIFY(!lease.allows("session", "first", first));
    QVERIFY(!lease.release("session", "first", first));
    bool wrote = false;
    QCOMPARE(lease.write("session", "first", first, [&wrote]() { wrote = true; return EditorStoreReply{}; }).status, 423);
    QVERIFY(!wrote);
    QVERIFY(lease.release("session", "second", second));
    const auto renewed = lease.acquire("session", "second").value("leaseToken").toString();
    QVERIFY(renewed != second);
    QVERIFY(!lease.release("session", "second", second));
    QVERIFY(lease.allows("session", "second", renewed));
}

void BrowserEditorServiceTest::rejectsSaveAfterWindowRelease()
{
    QTemporaryDir directory;
    ProcessorLibrary snapshot(directory.path(), true);
    BrowserEditorService service(snapshot);
    QSignalSpy opening(&service, &BrowserEditorService::openingLink);
    service.openEditor({}, "lua", false);
    QTRY_COMPARE(opening.size(), 1);
    QUrl url(opening.takeFirst().first().toString());
    const auto ticket = QUrlQuery(url.fragment()).queryItemValue("ticket");
    url.setFragment({}); url.setPath({});
    const auto origin = url.toString();
    const auto token = request(origin, "/sessions/exchange", "POST", {{"ticket", ticket}}).body.value("token").toString();
    m_window = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_leaseToken = request(origin, "/editor/lease", "POST", {{"windowId", m_window}}, token).body.value("leaseToken").toString();
    const auto created = request(origin, "/drafts", "POST", {{"languageId", "lua"}}, token);
    QCOMPARE(created.status, 201);
    qputenv("MQTT_PLUS_TEST_VALIDATOR_MODE", "delayed");
    const auto cleanup = qScopeGuard([]() { qunsetenv("MQTT_PLUS_TEST_VALIDATOR_MODE"); });
    const auto submitted = request(origin, "/drafts/" + created.body.value("draftId").toString() + "/save", "POST", {{"draftVersion", "1"}, {"requestId", "released-save"}}, token);
    QCOMPARE(submitted.status, 202);
    QCOMPARE(request(origin, "/editor/lease", "DELETE", {}, token).status, 200);
    QJsonObject result;
    QTRY_VERIFY_WITH_TIMEOUT((result = request(origin, "/jobs/" + submitted.body.value("jobId").toString(), "GET", {}, token).body).value("state") == "completed", 5000);
    QCOMPARE(result.value("status").toInt(), 423);
    QVERIFY(snapshot.processors().isEmpty());
    QCOMPARE(request(origin, "/drafts/" + created.body.value("draftId").toString(), "GET", {}, token).status, 200);
}
#include "test_browsereditorservice.moc"
