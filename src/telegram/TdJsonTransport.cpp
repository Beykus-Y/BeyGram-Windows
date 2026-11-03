#include "TdJsonTransport.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QLibrary>
#include <QProcessEnvironment>
#include <QThread>
#include <QtGlobal>

#include <atomic>
#include <functional>

namespace beygram::telegram {
namespace {

using CreateFn = void* (*)();
using SendFn = void (*)(void*, const char*);
using ReceiveFn = const char* (*)(void*, double);
using ExecuteFn = const char* (*)(const char*);
using DestroyFn = void (*)(void*);

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

QStringList libraryCandidates()
{
    QStringList candidates;
    const QString explicitPath =
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("BEYGRAM_TDLIB_DLL"));
    if (!explicitPath.isEmpty())
        candidates.append(explicitPath);

    const QString appDirectory = QCoreApplication::applicationDirPath();
    if (!appDirectory.isEmpty()) {
        candidates.append(QDir(appDirectory).filePath(QStringLiteral("tdjson")));
        candidates.append(QDir(appDirectory).filePath(QStringLiteral("tdjson.dll")));
    }
    candidates.append(QStringLiteral("tdjson"));
    candidates.removeDuplicates();
    return candidates;
}

} // namespace

struct TdJsonTransport::Impl {
    QLibrary library;
    CreateFn create = nullptr;
    SendFn send = nullptr;
    ReceiveFn receive = nullptr;
    ExecuteFn execute = nullptr;
    DestroyFn destroy = nullptr;

    void* client = nullptr;
    std::unique_ptr<ReceiverThread> receiver;
    QString loadError;
    bool loaded = false;

    void resetFunctions()
    {
        create = nullptr;
        send = nullptr;
        receive = nullptr;
        execute = nullptr;
        destroy = nullptr;
    }
};

TdJsonTransport::TdJsonTransport(QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>())
{
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

    QStringList attempts;
    for (const QString& candidate : libraryCandidates()) {
        attempts.append(candidate);
        d->library.setFileName(candidate);
        if (!d->library.load())
            continue;

        d->create = reinterpret_cast<CreateFn>(d->library.resolve("td_json_client_create"));
        d->send = reinterpret_cast<SendFn>(d->library.resolve("td_json_client_send"));
        d->receive = reinterpret_cast<ReceiveFn>(d->library.resolve("td_json_client_receive"));
        d->execute = reinterpret_cast<ExecuteFn>(d->library.resolve("td_json_client_execute"));
        d->destroy = reinterpret_cast<DestroyFn>(d->library.resolve("td_json_client_destroy"));

        if (d->create && d->send && d->receive && d->execute && d->destroy) {
            d->loaded = true;
            return true;
        }

        d->loadError = QStringLiteral("%1 loaded but does not export the TDLib JSON API")
                           .arg(candidate);
        d->resetFunctions();
        d->library.unload();
        break;
    }

    if (d->loadError.isEmpty()) {
        d->loadError = QStringLiteral(
                           "tdjson.dll was not found. Looked in: %1. "
                           "Set BEYGRAM_TDLIB_DLL or place tdjson.dll next to the executable.")
                           .arg(attempts.join(QStringLiteral(", ")));
    }

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
            this, [this, update = std::move(update)] { emit updateReceived(update); },
            Qt::QueuedConnection);
    };
    d->receiver->start();
    return true;
}

void TdJsonTransport::stop()
{
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

void TdJsonTransport::send(const QJsonObject& request)
{
    if (d->client == nullptr || !d->send) {
        emit transportError(QStringLiteral("TDLib client is not running"));
        return;
    }
    const QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
    d->send(d->client, payload.constData());
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
