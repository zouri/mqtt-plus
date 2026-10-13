#include "browsereditorservice.h"
#include "editorwindowlease.h"
#include "services/processors/editorcodec.h"
#include "services/storage/editordraftstore.h"

#include <QCborValue>
#include <QCoreApplication>
#include <QDateTime>
#include <QCryptographicHash>
#include <QLocale>
#include <cstring>
#include <QDesktopServices>
#include <QDirIterator>
#include <QFile>
#include <QHttpServer>
#include <QHttpServerConfiguration>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QPointer>
#include <QProcess>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>
#include <QtEndian>

namespace {
using Reply = std::function<void(EditorStoreReply)>;
QString identity() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString secret()
{
    QByteArray bytes(32, Qt::Uninitialized);
    for (int i = 0; i < 32; i += 4) {
        const quint32 value = QRandomGenerator::system()->generate();
        std::memcpy(bytes.data() + i, &value, 4);
    }
    return QString::fromLatin1(bytes.toHex());
}
EditorStoreReply fail(int code, const QString &message) { return {code, {{"error", message}}, {}, {}}; }

class LimitedTcpServer : public QTcpServer
{
public:
    using QTcpServer::QTcpServer;
protected:
    void incomingConnection(qintptr descriptor) override
    {
        auto socket = new QTcpSocket(this);
        if (!socket->setSocketDescriptor(descriptor)) { delete socket; return; }
        if (m_connections >= 16) { socket->abort(); delete socket; return; }
        ++m_connections;
        connect(socket, &QObject::destroyed, this, [this]() { --m_connections; });
        addPendingConnection(socket);
        emit newConnection();
    }
private:
    int m_connections = 0;
};
}

class EditorStorageWorker : public QObject
{
public:
    std::unique_ptr<EditorDraftStore> store;
};

class EditorHttpWorker : public QObject
{
public:
    EditorHttpWorker(EditorStorageWorker *storage, BrowserEditorService *owner)
        : m_storage(storage), m_owner(owner) {}

    void storage(std::function<EditorStoreReply(EditorDraftStore &)> operation, Reply reply)
    {
        QPointer<EditorHttpWorker> guard(this);
        QMetaObject::invokeMethod(m_storage, [worker = m_storage, guard, operation, reply]() {
            auto result = worker->store ? operation(*worker->store) : fail(503, QStringLiteral("Editor storage is unavailable."));
            if (guard) QMetaObject::invokeMethod(guard, [guard, reply, result]() {
                if (guard && !guard->m_stopping) reply(result);
            });
        });
    }

    void setAppearance(const QString &language, const QString &theme, const QString &font)
    {
        m_appearance = {{"language", language == "system" ? QLocale().name() : language}, {"theme", theme}, {"font", font}};
    }

    void open(const QString &processorId, const QString &language, bool launchBrowser)
    {
        if (!start()) return;
        const auto currentTime = QDateTime::currentMSecsSinceEpoch();
        for (auto it = m_tickets.begin(); it != m_tickets.end();) {
            if (it->expires <= currentTime) it = m_tickets.erase(it); else ++it;
        }
        if (m_tickets.size() >= 100 || m_sessions.size() >= 1000) return;
        const auto token = secret();
        m_tickets.insert(token, {QDateTime::currentMSecsSinceEpoch() + 60000,
            {{"processorId", processorId}, {"languageId", language}}});
        const QString url = m_origin + QStringLiteral("/#ticket=%1").arg(token);
        QMetaObject::invokeMethod(m_owner, [owner = m_owner, url, launchBrowser]() {
            if (!owner) return;
            emit owner->openingLink(url);
            if (launchBrowser && !QDesktopServices::openUrl(QUrl(url))) emit owner->errorOccurred(QCoreApplication::translate("BrowserEditorService", "Cannot open the browser. Copy the editor link to open it manually."));
        });
    }

    void stop()
    {
        m_stopping = true;
        if (m_tcp) m_tcp->close();
        if (m_process) { m_process->kill(); m_process->waitForFinished(1000); }
        m_tickets.clear();
        m_sessions.clear();
        m_queue.clear();
        m_jobs.clear();
    }

    void removeProcessor(const QString &id)
    {
        storage([id](EditorDraftStore &store) { return store.deleteProcessor(id); }, [this](EditorStoreReply result) {
            apply(result, [this, result]() {
                if (result.status != 200) QMetaObject::invokeMethod(m_owner, [owner = m_owner]() {
                    if (owner) emit owner->errorOccurred(QCoreApplication::translate("BrowserEditorService", "Cannot delete processor."));
                });
            });
        });
    }

    void background(const QStringList &ids)
    {
        m_background = ids;
        next();
    }

private:
    struct Ticket { qint64 expires = 0; QJsonObject target; };
    struct Job { QString owner; QString operation; QString requestId; QJsonObject draft; QJsonObject result; QString window; QString leaseToken; };
    EditorStorageWorker *m_storage;
    QPointer<BrowserEditorService> m_owner;
    QHttpServer *m_server = nullptr;
    LimitedTcpServer *m_tcp = nullptr;
    QString m_origin;
    QString m_host;
    QHash<QString, Ticket> m_tickets;
    QHash<QString, QJsonObject> m_sessions;
    std::shared_ptr<EditorWindowLease> m_lease = std::make_shared<EditorWindowLease>();
    QSet<QString> m_assets;
    QHash<QString, Job> m_jobs;
    QStringList m_queue;
    QStringList m_background;
    QHash<QString, QJsonObject> m_validationCache;
    QStringList m_cacheOrder;
    QProcess *m_process = nullptr;
    QString m_active;
    bool m_stopping = false;
    int m_pendingFreezes = 0;
    QJsonObject m_appearance{{"language", QLocale().name()}, {"theme", "system"}, {"font", "sans-serif"}};

    bool start()
    {
        if (m_server) return true;
        m_server = new QHttpServer(this);
        m_tcp = new LimitedTcpServer(m_server);
        QHttpServerConfiguration config;
        config.setMaximumBodySize(16 * 1024 * 1024);
        config.setMaximumTotalHeaderSize(16 * 1024);
        config.setMaximumUrlSize(4096);
        config.setRateLimitPerSecond(30);
        m_server->setConfiguration(config);
        if (!m_tcp->listen(QHostAddress::LocalHost, 0) || !m_server->bind(m_tcp)) {
            delete m_server;
            m_server = nullptr;
            m_tcp = nullptr;
            QMetaObject::invokeMethod(m_owner, [owner = m_owner]() {
                if (owner) emit owner->errorOccurred(QCoreApplication::translate("BrowserEditorService", "Cannot start the local script editor."));
            });
            return false;
        }
        m_host = QStringLiteral("127.0.0.1:%1").arg(m_tcp->serverPort());
        m_origin = QStringLiteral("http://") + m_host;
        QDirIterator files(QStringLiteral(":/processor-editor"), QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) m_assets.insert(files.next().sliced(QStringLiteral(":/processor-editor").size()));
        m_server->setMissingHandler(this, [this](const QHttpServerRequest &request, QHttpServerResponder &responder) {
            handle(request, std::make_shared<QHttpServerResponder>(std::move(responder)));
        });
        return true;
    }

    void respond(const std::shared_ptr<QHttpServerResponder> &responder, const EditorStoreReply &reply)
    {
        QHttpHeaders headers;
        headers.append("Content-Type", "application/json; charset=utf-8");
        headers.append("Cache-Control", "no-store");
        headers.append("X-Content-Type-Options", "nosniff");
        headers.append("Content-Security-Policy", "default-src 'none'; frame-ancestors 'none'");
        responder->write(QJsonDocument(reply.data), headers, QHttpServerResponder::StatusCode(reply.status));
    }

    void handle(const QHttpServerRequest &request, const std::shared_ptr<QHttpServerResponder> &responder)
    {
        auto answer = [this, responder](EditorStoreReply reply) { respond(responder, reply); };
        if (m_stopping) { answer(fail(503, QStringLiteral("Editor is shutting down."))); return; }
        if (request.value("Host") != m_host.toUtf8()) { answer(fail(403, QStringLiteral("Invalid host."))); return; }
        const auto path = request.url().path();
        const auto method = request.method();
        const bool get = method == QHttpServerRequest::Method::Get;
        const QString verb = get ? "GET" : method == QHttpServerRequest::Method::Post ? "POST"
            : method == QHttpServerRequest::Method::Put ? "PUT" : method == QHttpServerRequest::Method::Delete ? "DELETE" : "";
        if (verb.isEmpty()) { answer(fail(405, QStringLiteral("Unsupported method."))); return; }
        if (!path.startsWith("/api/v1/")) {
            if (path == "/favicon.ico" && get) {
                responder->write(QHttpServerResponder::StatusCode::NoContent);
                return;
            }
            const auto asset = path == "/" ? QStringLiteral("/index.html") : path;
            if (!get || !m_assets.contains(asset)) { answer(fail(404, QStringLiteral("Not found."))); return; }
            QFile file(QStringLiteral(":/processor-editor") + asset);
            if (!file.open(QIODevice::ReadOnly)) { answer(fail(404, QStringLiteral("Not found."))); return; }
            QHttpHeaders headers;
            const auto mime = QMimeDatabase().mimeTypeForFile(asset).name().toUtf8();
            headers.append("Content-Type", mime);
            headers.append("X-Content-Type-Options", "nosniff");
            headers.append("Referrer-Policy", "no-referrer");
            headers.append("Cache-Control", asset.startsWith(QStringLiteral("/assets/"))
                ? "public, max-age=31536000, immutable" : "no-store");
            headers.append("Content-Security-Policy", "default-src 'self'; script-src 'self'; worker-src 'self' blob:; style-src 'self' 'unsafe-inline'; font-src 'self' data:; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; object-src 'none'");
            responder->write(file.readAll(), headers);
            return;
        }
        if (!get && request.value("Origin") != m_origin.toUtf8()) { answer(fail(403, QStringLiteral("Invalid origin."))); return; }
        QJsonObject body;
        if (verb == "POST" || verb == "PUT") {
            if (request.value("Content-Type").split(';').first() != "application/json") {
                answer(fail(415, QStringLiteral("JSON content is required."))); return;
            }
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(request.body(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) {
                answer(fail(400, QStringLiteral("Invalid JSON."))); return;
            }
            body = document.object();
        }
        const auto route = path.sliced(7);
        if (route == "/sessions/exchange" && verb == "POST") {
            const auto key = body.value("ticket").toString();
            const auto ticket = m_tickets.take(key);
            if (ticket.expires <= QDateTime::currentMSecsSinceEpoch()) { answer(fail(401, QStringLiteral("Editor link expired. Reopen it from the program."))); return; }
            const auto token = secret();
            m_sessions.insert(token, ticket.target);
            answer({200, {{"token", token}, {"target", ticket.target}}, {}, {}});
            return;
        }
        const auto authorization = request.value("Authorization");
        const QString session = authorization.startsWith("Bearer ") ? QString::fromLatin1(authorization.sliced(7)) : QString();
        if (!m_sessions.contains(session)) { answer(fail(401, QStringLiteral("Reopen the editor from the program."))); return; }
        const auto window = QString::fromLatin1(request.value("X-Editor-Window"));
        const auto leaseToken = QString::fromLatin1(request.value("X-Editor-Lease"));
        if (route == "/editor/lease") {
            if (verb == "POST") {
                const auto id = body.value("windowId").toString();
                if (id.size() > 128 || QUuid(id).isNull()) { answer(fail(422, QStringLiteral("A window identity is required."))); return; }
                answer({200, m_lease->acquire(session, id), {}, {}});
            } else if (verb == "DELETE") {
                if (!m_lease->release(session, window, leaseToken)) { answer(fail(423, QStringLiteral("This window does not own editing."))); return; }
                answer({200, {{"released", true}}, {}, {}});
            } else answer(fail(405, QStringLiteral("Unsupported method.")));
            return;
        }
        if (!get && !m_lease->allows(session, window, leaseToken)) {
            answer(fail(423, QStringLiteral("Another window owns editing. This window is read-only.")));
            return;
        }
        if (route == "/bootstrap" && get) {
            auto bootstrap = m_appearance;
            bootstrap.insert("protocolVersion", 1);
            bootstrap.insert("templates", QJsonArray{EditorCodec::templateDraft("lua"), EditorCodec::templateDraft("javascript")});
            bootstrap.insert("limits", QJsonObject{{"maxFiles", 64}, {"maxFileBytes", 1048576}, {"maxTotalBytes", 2097152}});
            answer({200, bootstrap, {}, {}});
            return;
        }
        if (route.startsWith("/jobs/")) {
            const auto id = route.sliced(6);
            if (!m_jobs.contains(id) || m_jobs[id].owner != session) { answer(fail(404, QStringLiteral("Job is unavailable."))); return; }
            if (verb == "DELETE") {
                auto &job = m_jobs[id];
                const auto state = job.result.value("state").toString();
                if (state == "committing" || state == "completed") { answer(fail(409, QStringLiteral("Save has already entered commit."))); return; }
                job.result = {{"state", "cancelled"}};
                job.draft = {};
                m_queue.removeAll(id);
                if (id == m_active && m_process) m_process->kill();
            } else if (!get) { answer(fail(405, QStringLiteral("Unsupported method."))); return; }
            answer({200, m_jobs[id].result, {}, {}});
            return;
        }
        if (route == "/jobs" && get) {
            const auto requestId = QUrlQuery(request.url()).queryItemValue(QStringLiteral("requestId"));
            for (auto it = m_jobs.cbegin(); it != m_jobs.cend(); ++it) {
                if (!requestId.isEmpty() && it->requestId == requestId && it->owner == session) {
                    answer({200, {{"jobId", it.key()}}, {}, {}});
                    return;
                }
            }
            answer(fail(404, QStringLiteral("Job is unavailable.")));
            return;
        }
        const auto parts = route.split('/');
        if (parts.size() == 4 && parts[1] == "drafts" && verb == "POST"
            && (parts[3] == "validate" || parts[3] == "save")) {
            if (m_jobs.size() >= 32) {
                for (auto it = m_jobs.begin(); it != m_jobs.end();) {
                    const auto state = it->result.value("state").toString();
                    if (it.key() != m_active && (state == "completed" || state == "cancelled")) it = m_jobs.erase(it); else ++it;
                }
            }
            int outstanding = m_pendingFreezes;
            for (const auto &job : std::as_const(m_jobs)) {
                const auto state = job.result.value("state").toString();
                if (state == "queued" || state == "validating" || state == "committing") ++outstanding;
            }
            if (m_queue.size() + m_pendingFreezes >= 8 || outstanding >= 9) { answer(fail(429, QStringLiteral("Editor job queue is full."))); return; }
            const auto requestId = body.value("requestId").toString();
            if (parts[3] == "save" && (requestId.isEmpty() || requestId.size() > 128)) { answer(fail(422, QStringLiteral("Save identity is required."))); return; }
            ++m_pendingFreezes;
            storage([id = parts[2]](EditorDraftStore &store) { return store.request("GET", "/drafts/" + id); },
                [this, answer, body, session, window, leaseToken, operation = parts[3], requestId](EditorStoreReply result) {
                    --m_pendingFreezes;
                    if (!m_lease->allows(session, window, leaseToken)) { answer(fail(423, QStringLiteral("Editing permission expired before submission."))); return; }
                    if (result.status != 200) { answer(result); return; }
                    if (result.data.value("draftVersion") != body.value("draftVersion")) { answer(fail(409, QStringLiteral("Draft changed before submission."))); return; }
                    if (m_queue.size() >= 8) { answer(fail(429, QStringLiteral("Editor job queue is full."))); return; }
                    const auto id = identity();
                    m_jobs.insert(id, {session, operation, requestId, result.data, {{"state", "queued"}}, window, leaseToken});
                    m_queue.append(id);
                    answer({202, {{"jobId", id}}, {}, {}});
                    next();
                });
            return;
        }
        const auto version = QString::fromLatin1(request.value("If-Match")).remove('"');
        const auto processorId = QUrlQuery(request.url()).queryItemValue(QStringLiteral("processorId"));
        storage([verb, route, body, version, processorId, session, window, leaseToken, lease = m_lease](EditorDraftStore &store) {
            const auto operation = [&]() { return store.request(verb, route, body, version, processorId); };
            return verb == "GET" ? operation() : lease->write(session, window, leaseToken, operation);
        }, answer);
    }

    void apply(EditorStoreReply result, std::function<void()> finished)
    {
        if (!result.saved.ok && result.deletedId.isEmpty()) { finished(); return; }
        QPointer<EditorHttpWorker> guard(this);
        QMetaObject::invokeMethod(m_owner, [owner = m_owner, guard, result, finished]() {
            if (owner) owner->applyMutation(result.saved, result.deletedId, [guard, finished]() {
                if (guard) QMetaObject::invokeMethod(guard, [guard, finished]() { if (guard && !guard->m_stopping) finished(); });
            });
        });
    }

    QString cacheKey(const QJsonObject &draft)
    {
        auto content = draft;
        for (const auto &key : {"draftId", "draftVersion", "updatedAt", "processorId", "baseVersion", "revisionId"}) content.remove(key);
        return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(content).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    }

    void next()
    {
        if (m_stopping || m_process) return;
        if (m_queue.isEmpty()) {
            if (m_background.isEmpty() || m_loadingBackground) return;
            m_loadingBackground = true;
            const auto processorId = m_background.takeFirst();
            storage([processorId](EditorDraftStore &store) { return store.request("GET", "/processors/" + processorId); },
                [this, processorId](EditorStoreReply result) {
                    m_loadingBackground = false;
                    if (result.status == 200) {
                        const auto id = identity();
                        auto draft = result.data.value("package").toObject();
                        draft.insert("processorId", processorId);
                        draft.insert("revisionId", result.data.value("revisionId"));
                        m_jobs.insert(id, {{}, "background", {}, draft, {{"state", "queued"}}});
                        m_queue.append(id);
                    }
                    next();
                });
            return;
        }
        const auto id = m_queue.takeFirst();
        m_active = id;
        m_jobs[id].result = {{"state", "validating"}};
        const auto key = cacheKey(m_jobs[id].draft);
        if (m_validationCache.contains(key)) { validated(id, m_validationCache[key]); return; }
        auto process = new QProcess(this);
        m_process = process;
        auto timer = new QTimer(process);
        timer->setSingleShot(true);
        auto output = std::make_shared<QByteArray>();
        auto timedOut = std::make_shared<bool>(false);
        connect(timer, &QTimer::timeout, process, [process, timedOut]() { *timedOut = true; process->kill(); });
        connect(process, &QProcess::readyReadStandardOutput, process, [process, output]() {
            output->append(process->readAllStandardOutput());
            if (output->size() > 128 * 1024) process->kill();
        });
        connect(process, &QProcess::readyReadStandardError, process, [process]() { process->readAllStandardError(); });
        auto finished = [this, process, timer, output, timedOut, id, key](bool normal) {
            if (m_process != process) return;
            timer->stop();
            output->append(process->readAllStandardOutput());
            m_process = nullptr;
            process->deleteLater();
            if (m_stopping) return;
            QJsonObject result{{"ok", false}, {"diagnostics", QJsonArray{QJsonObject{{"code", "validator_failed"},
                {"message", *timedOut ? "Validation timed out." : "Validator exited unexpectedly."}}}}};
            if (normal && output->size() >= 4) {
                const auto length = qFromBigEndian<quint32>(output->constData());
                if (length <= 128 * 1024 && output->size() == length + 4) {
                    QCborParserError error;
                    const auto value = QCborValue::fromCbor(output->sliced(4), &error);
                    if (error.error == QCborError::NoError && value.isMap()) result = value.toJsonValue().toObject();
                }
            }
            if (result.value("ok").toBool()) {
                m_validationCache.insert(key, result);
                m_cacheOrder.append(key);
                if (m_cacheOrder.size() > 128) m_validationCache.remove(m_cacheOrder.takeFirst());
            }
            validated(id, result);
        };
        connect(process, &QProcess::errorOccurred, process, [finished](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) finished(false);
        });
        connect(process, &QProcess::finished, process, [finished](int code, QProcess::ExitStatus status) {
            finished(code == 0 && status == QProcess::NormalExit);
        });
        connect(process, &QProcess::started, process, [process, draft = m_jobs[id].draft]() {
            const auto data = QCborValue::fromJsonValue(draft).toCbor();
            QByteArray frame(4, Qt::Uninitialized);
            qToBigEndian(quint32(data.size()), frame.data());
            process->write(frame + data);
            process->closeWriteChannel();
        });
        timer->start(5000);
        process->start(QCoreApplication::applicationFilePath(), {QStringLiteral("--processor-validator")});
    }

    void validated(const QString &id, const QJsonObject &validation)
    {
        m_active.clear();
        if (!m_jobs.contains(id)) { next(); return; }
        auto &job = m_jobs[id];
        if (job.result.value("state") == "cancelled") { job.draft = {}; next(); return; }
        if (job.operation == "background") {
            QStringList messages;
            for (const auto &item : validation.value("diagnostics").toArray()) messages.append(item.toObject().value("message").toString());
            const auto processorId = job.draft.value("processorId").toString();
            const auto revisionId = job.draft.value("revisionId").toString();
            const auto ready = validation.value("ok").toBool();
            QMetaObject::invokeMethod(m_owner, [owner = m_owner, processorId, revisionId, ready, detail = messages.join('\n')]() {
                if (owner) emit owner->readinessChanged(processorId, revisionId, ready, detail);
            });
            m_jobs.remove(id);
            next();
            return;
        }
        if (!validation.value("ok").toBool() || job.operation == "validate") {
            job.result = {{"state", "completed"}, {"status", validation.value("ok").toBool() ? 200 : 422}, {"validation", validation}};
            job.draft = {};
            next();
            return;
        }
        job.result = {{"state", "committing"}};
        storage([draft = job.draft, requestId = job.requestId, session = job.owner, window = job.window,
            leaseToken = job.leaseToken, lease = m_lease](EditorDraftStore &store) {
            return lease->write(session, window, leaseToken, [&]() { return store.save(draft, requestId); });
        },
            [this, id, validation](EditorStoreReply result) {
                apply(result, [this, id, result, validation]() {
                    auto receipt = result.data;
                    receipt.remove("draft");
                    m_jobs[id].result = {{"state", "completed"}, {"status", result.status}, {"validation", validation}, {"result", receipt}};
                    m_jobs[id].draft = {};
                });
            });
        next();
    }
    bool m_loadingBackground = false;
};

BrowserEditorService::BrowserEditorService(ProcessorLibrary &snapshot, QObject *parent)
    : QObject(parent), m_snapshot(snapshot)
{
    m_storage = new EditorStorageWorker;
    m_http = new EditorHttpWorker(m_storage, this);
    m_storage->moveToThread(&m_storageThread);
    m_http->moveToThread(&m_httpThread);
    connect(&m_storageThread, &QThread::finished, m_storage, &QObject::deleteLater);
    connect(&m_httpThread, &QThread::finished, m_http, &QObject::deleteLater);
    m_storageThread.start();
    m_httpThread.start();
    QMetaObject::invokeMethod(m_storage, [this, directory = snapshot.storageDirectory()]() {
        m_storage->store = std::make_unique<EditorDraftStore>(directory);
        const auto processors = m_storage->store->library().processors();
        const bool ready = m_storage->store->isReady();
        QVector<QSharedPointer<const ProcessorRevisionSnapshot>> revisions;
        for (const auto &processor : processors) revisions += m_storage->store->library().revisions(processor.id);
        QMetaObject::invokeMethod(this, [this, processors, revisions, ready]() {
            m_snapshot.replaceSnapshot(processors, revisions);
            emit libraryChanged();
            if (!ready) emit errorOccurred(tr("Cannot load Processor Library."));
        });
        QStringList ids;
        for (const auto &processor : processors) ids.append(processor.id);
        QMetaObject::invokeMethod(m_http, [this, ids]() { m_http->background(ids); });
    });
}

BrowserEditorService::~BrowserEditorService()
{
    QMetaObject::invokeMethod(m_http, [this]() { m_http->stop(); }, Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_storage, [this]() { m_storage->store.reset(); }, Qt::BlockingQueuedConnection);
    m_storageThread.quit();
    m_storageThread.wait();
    m_httpThread.quit();
    m_httpThread.wait();
}

void BrowserEditorService::openEditor(const QString &processorId, const QString &language, bool launchBrowser)
{
    QMetaObject::invokeMethod(m_http, [this, processorId, language, launchBrowser]() { m_http->open(processorId, language, launchBrowser); });
}

void BrowserEditorService::setAppearance(const QString &language, const QString &theme, const QString &font)
{
    QMetaObject::invokeMethod(m_http, [this, language, theme, font]() { m_http->setAppearance(language, theme, font); });
}

void BrowserEditorService::deleteProcessor(const QString &processorId)
{
    QMetaObject::invokeMethod(m_http, [this, processorId]() { m_http->removeProcessor(processorId); });
}

void BrowserEditorService::applyMutation(const SaveProcessorRevisionResult &saved, const QString &deletedId,
    std::function<void()> acknowledge)
{
    if (saved.ok) m_snapshot.applySavedSnapshot(saved);
    if (!deletedId.isEmpty()) m_snapshot.applyDeletedSnapshot(deletedId);
    emit libraryChanged();
    if (saved.ok) emit readinessChanged(saved.processor.id, saved.revision->id, true, {});
    acknowledge();
}

void BrowserEditorService::requestDraftCounts()
{
    QMetaObject::invokeMethod(m_http, [this]() {
        m_http->storage([](EditorDraftStore &store) {
            return EditorStoreReply{200, store.draftCounts(), {}, {}};
        }, [this](EditorStoreReply reply) {
            QMetaObject::invokeMethod(this, [this, reply]() { emit draftCountsChanged(reply.data); });
        });
    });
}
