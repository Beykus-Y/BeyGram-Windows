#pragma once

#include "core/Result.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

namespace beygram::telegram {

/// Thin, thread-safe facade over TDLib's official JSON interface (tdjson).
///
/// The DLL is loaded from a trusted location only (see TdLibLocator). Requests
/// are asynchronous and carry a generated `@extra` id so responses can be
/// correlated even when several requests are in flight. Only td_execute (used
/// for local operations) is synchronous.
class TdJsonTransport final : public QObject {
    Q_OBJECT

public:
    using RequestId = quint64;
    using ResponseHandler = std::function<void(const QJsonObject&)>;

    explicit TdJsonTransport(QObject* parent = nullptr);
    ~TdJsonTransport() override;

    TdJsonTransport(const TdJsonTransport&) = delete;
    TdJsonTransport& operator=(const TdJsonTransport&) = delete;

    /// Attempts to load tdjson from a trusted location. Returns true when all
    /// required exports exist.
    bool load(QString* error = nullptr);
    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] QString loadError() const;
    [[nodiscard]] QString loadedPath() const;
    [[nodiscard]] bool loadedFromApplicationDirectory() const;

    /// Creates the TDLib client and starts the receiver thread.
    bool start(QString* error = nullptr);
    void stop();
    [[nodiscard]] bool isRunning() const;

    /// Sends a request and correlates its response through `@extra`.
    /// `timeoutMs` of 0 disables the timeout (use only for requests that are
    /// bounded by their own cancellation path).
    RequestId sendRequest(const QJsonObject& request, ResponseHandler handler = {},
                          int timeoutMs = 30000);

    /// Sends a fire-and-forget request without correlation.
    void send(const QJsonObject& request);

    /// Cancels one pending request; its handler receives a synthetic error.
    bool cancelRequest(RequestId id);

    /// Cancels every pending request (used on shutdown). When `notifyHandlers`
    /// is false the handlers are discarded, which is required while the owning
    /// session is being destroyed.
    void cancelAllRequests(bool notifyHandlers = true);

    [[nodiscard]] int pendingRequestCount() const;

    /// Executes a request synchronously in TDLib. Only for local operations
    /// such as option inspection; never use it for network requests.
    [[nodiscard]] Result<QJsonObject> execute(const QJsonObject& request);

signals:
    void updateReceived(const QJsonObject& update);
    void responseReceived(quint64 requestId, const QJsonObject& response);
    void requestTimedOut(quint64 requestId);
    void requestCancelled(quint64 requestId);
    void transportError(const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace beygram::telegram

Q_DECLARE_METATYPE(beygram::telegram::TdJsonTransport::RequestId)
