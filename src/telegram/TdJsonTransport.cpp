#include "TdJsonTransport.h"

#include "TdLibLocator.h"

#include <QJsonDocument>
#include <QLibrary>
#include <QLoggingCategory>
#include <QTimer>
#include <QThread>
#include <QtGlobal>

#include <atomic>
#include <functional>

namespace beygram::telegram {
namespace {
Q_LOGGING_CATEGORY(lcTransport, "beygram.telegram.transport")

using CreateFn = void* (*)();
using SendFn = void (*)(void*, const char*);
using ReceiveFn = const char* (*)(void*, double);
using ExecuteFn = const char* (*)(const char*);
using DestroyFn = void (*)(void*);
using SetLogVerbosityFn = void (*)(int);

/// Polls td_receive on a worker thread. td_receive itself is blocking only up
/// to its timeout, so the thread needs no event loop.
class ReceiverThread final : public QThread {
public:
    void* client = nullptr;
    ReceiveFn receive = nullptr;
    std::function<void(QJsonObject)> onUpdate;
    std::atomic_bool stopped{false};

protected:
    void run() override
    {
        while (!stopped.load(std::memory_order_relaxed)) {
            if (!receive || client == nullptr)
                break;
            const char* text = receive(client, 0.1);
            if (text == nullptr)
                continue;

            const QJsonDocument document = QJsonDocument::fromJson(QByteArray(text));
            if (document.isObject() && onUpdate)
                onUpdate(document.object());
        }
    }
};

QJsonObject syntheticError(const QString& message, int code = -1)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("error")},
        {QStringLiteral("code"), code},
        {QStringLiteral("message"), message},
    };
}

} // namespace

struct TdJsonTransport::Impl {
    QLibrary library;
    CreateFn create = nullptr;
    SendFn send = nullptr;
    ReceiveFn receive = nullptr;
    ExecuteFn execute = nullptr;
    DestroyFn destroy = nullptr;
    SetLogVerbosityFn setLogVerbosity = nullptr;

    void* client = nullptr;
    std::unique_ptr<ReceiverThread> receiver;
    QString loadError;
    QString loadedPath;
    bool loadedFromApplication = false;
    bool loaded = false;

    struct Pending {
        ResponseHandler handler;
        QTimer* timer = nullptr;
    };
    TdJsonTransport* owner = nullptr;
    QHash<RequestId, Pending> pending;
    RequestId nextRequestId = 1;

    void resetFunctions()
    {
        create = nullptr;
        send = nullptr;
        receive = nullptr;
        execute = nullptr;
        destroy = nullptr;
        setLogVerbosity = nullptr;
    }

    void finish(QHash<RequestId, Pending>::iterator it, const QJsonObject& response)
    {
        if (it->timer) {
            it->timer->stop();
            it->timer->deleteLater();
        }
        ResponseHandler handler = std::move(it->handler);
        const RequestId id = it.key();
        pending.erase(it);
        if (handler)
            handler(response);
        owner->responseReceived(id, response);
    }
};

TdJsonTransport::TdJsonTransport(QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>())
{
    d->owner = this;
}

TdJsonTransport::~TdJsonTransport()
{
    stop();
    if (d->library.isLoaded())
        d->library.unload();
}

bool TdJsonTransport::load(QString* error)
{
    if (d->loaded)
        return true;

    d->loadError.clear();
    d->resetFunctions();

    if (d->library.isLoaded())
        d->library.unload();
    d->library.setFileName(QString());

    const auto location = locateTdlibLibrary();
    if (!location.hasValue()) {
        d->loadError = location.error().message;
        if (error)
            *error = d->loadError;
        return false;
    }

    d->loadedPath = location.value().path;
    d->loadedFromApplication = location.value().applicationOwned;
    d->library.setFileName(d->loadedPath);

    if (!d->library.load()) {
        d->loadError = QStringLiteral("Could not load %1: %2")
                           .arg(d->loadedPath, d->library.errorString());
        if (error)
            *error = d->loadError;
        return false;
    }

    d->create = reinterpret_cast<CreateFn>(d->library.resolve("td_json_client_create"));
    d->send = reinterpret_cast<SendFn>(d->library.resolve("td_json_client_send"));
    d->receive = reinterpret_cast<ReceiveFn>(d->library.resolve("td_json_client_receive"));
    d->execute = reinterpret_cast<ExecuteFn>(d->library.resolve("td_json_client_execute"));
    d->destroy = reinterpret_cast<DestroyFn>(d->library.resolve("td_json_client_destroy"));
    d->setLogVerbosity =
        reinterpret_cast<SetLogVerbosityFn>(d->library.resolve("td_set_log_verbosity_level"));

    if (d->create && d->send && d->receive && d->execute && d->destroy) {
        d->loaded = true;
        // TDLib logs to stderr by default and may include request details.
        // Disable its internal log before creating any client; BeyGram keeps
        // its own redacted log. td_set_log_verbosity_level is the C API; using
        // td_execute for this is not supported by TDLib.
        if (d->setLogVerbosity)
            d->setLogVerbosity(0);
        return true;
    }

    d->loadError = QStringLiteral("%1 loaded but does not export the TDLib JSON API")
                       .arg(d->loadedPath);
    d->resetFunctions();
    d->library.unload();
    if (error)
        *error = d->loadError;
    return false;
}

bool TdJsonTransport::isAvailable() const
{
    return d->loaded;
}

QString TdJsonTransport::loadError() const
{
    return d->loadError;
}

QString TdJsonTransport::loadedPath() const
{
    return d->loadedPath;
}

bool TdJsonTransport::loadedFromApplicationDirectory() const
{
    return d->loadedFromApplication;
}

bool TdJsonTransport::start(QString* error)
{
    if (d->client != nullptr && d->receiver && d->receiver->isRunning())
        return true;

    if (!load(error))
        return false;

    d->client = d->create();
    if (d->client == nullptr) {
        const QString message = QStringLiteral("TDLib refused to create a client");
        if (error)
            *error = message;
        d->loadError = message;
        return false;
    }

    d->receiver = std::make_unique<ReceiverThread>();
    d->receiver->client = d->client;
    d->receiver->receive = d->receive;
    d->receiver->onUpdate = [this](QJsonObject update) {
        QMetaObject::invokeMethod(
            this, [this, update = std::move(update)] { 
                const QJsonValue extra = update.value(QStringLiteral("@extra"));
                qCDebug(lcTransport) << "recv" << update.value(QStringLiteral("@type")).toString()
                                     << "extra" << extra;
                if (extra.isDouble()) {
                    const auto id = static_cast<RequestId>(extra.toVariant().toULongLong());
                    auto it = d->pending.find(id);
                    if (it != d->pending.end()) {
                        d->finish(it, update);
                        return;
                    }
                }
                emit updateReceived(update);
            },
            Qt::QueuedConnection);
    };
    d->receiver->start();
    return true;
}

void TdJsonTransport::stop()
{
    cancelAllRequests(false);

    if (d->receiver) {
        d->receiver->stopped.store(true, std::memory_order_relaxed);
        // td_receive uses a 100 ms timeout, so this completes even while TDLib
        // is idle. Waiting without a deadline is deliberate: destroying a
        // running QThread is undefined behaviour.
        d->receiver->wait();
        d->receiver.reset();
    }

    if (d->client != nullptr) {
        if (d->destroy)
            d->destroy(d->client);
        d->client = nullptr;
    }
}

bool TdJsonTransport::isRunning() const
{
    return d->client != nullptr;
}

TdJsonTransport::RequestId TdJsonTransport::sendRequest(const QJsonObject& request,
                                                       ResponseHandler handler, int timeoutMs)
{
    if (d->client == nullptr || !d->send) {
        const QJsonObject error = syntheticError(QStringLiteral("TDLib client is not running"));
        if (handler)
            handler(error);
        emit transportError(QStringLiteral("TDLib client is not running"));
        return 0;
    }

    const RequestId id = d->nextRequestId++;
    QJsonObject payload = request;
    payload.insert(QStringLiteral("@extra"), static_cast<double>(id));

    Impl::Pending pending;
    pending.handler = std::move(handler);
    if (timeoutMs > 0) {
        pending.timer = new QTimer(this);
        pending.timer->setSingleShot(true);
        connect(pending.timer, &QTimer::timeout, this, [this, id] {
            auto it = d->pending.find(id);
            if (it == d->pending.end())
                return;
            emit requestTimedOut(id);
            d->finish(it, syntheticError(QStringLiteral("TDLib request timed out"), -2));
        });
        pending.timer->start(timeoutMs);
    }
    d->pending.insert(id, pending);

    const QByteArray bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    d->send(d->client, bytes.constData());
    return id;
}

void TdJsonTransport::send(const QJsonObject& request)
{
    if (d->client == nullptr || !d->send) {
        emit transportError(QStringLiteral("TDLib client is not running"));
        return;
    }
    const QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
    d->send(d->client, payload.constData());
}

bool TdJsonTransport::cancelRequest(RequestId id)
{
    auto it = d->pending.find(id);
    if (it == d->pending.end())
        return false;
    emit requestCancelled(id);
    d->finish(it, syntheticError(QStringLiteral("TDLib request cancelled"), -3));
    return true;
}

void TdJsonTransport::cancelAllRequests(bool notifyHandlers)
{
    if (d->pending.isEmpty())
        return;

    const auto ids = d->pending.keys();
    for (RequestId id : ids) {
        if (!notifyHandlers) {
            auto it = d->pending.find(id);
            if (it == d->pending.end())
                continue;
            if (it->timer) {
                it->timer->stop();
                it->timer->deleteLater();
            }
            d->pending.erase(it);
            continue;
        }
        cancelRequest(id);
    }
}

int TdJsonTransport::pendingRequestCount() const
{
    return d->pending.size();
}

Result<QJsonObject> TdJsonTransport::execute(const QJsonObject& request)
{
    if (!load())
        return Result<QJsonObject>::fail(ErrorCode::Unavailable, d->loadError);
    if (!d->execute)
        return Result<QJsonObject>::fail(ErrorCode::Unavailable,
                                         QStringLiteral("td_json_client_execute is missing"));

    const QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
    const char* response = d->execute(payload.constData());
    if (response == nullptr)
        return Result<QJsonObject>::fail(ErrorCode::Tdlib,
                                         QStringLiteral("TDLib returned an empty response"));

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(response), &parseError);
    if (!document.isObject())
        return Result<QJsonObject>::fail(
            ErrorCode::Tdlib,
            QStringLiteral("TDLib returned malformed JSON: %1").arg(parseError.errorString()));
    return Result<QJsonObject>::ok(document.object());
}

} // namespace beygram::telegram
