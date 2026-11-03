#pragma once

#include "core/Result.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <memory>

namespace beygram::telegram {

/// Thin, thread-safe facade over TDLib's official JSON interface (tdjson).
///
/// The DLL is loaded at runtime, so the desktop client builds and starts
/// without TDLib and reports the absence honestly. Requests are asynchronous;
/// only td_execute (used for local validation, never for network access) is
/// synchronous. Updates arrive on the GUI thread through updateReceived().
class TdJsonTransport final : public QObject {
    Q_OBJECT

public:
    explicit TdJsonTransport(QObject* parent = nullptr);
    ~TdJsonTransport() override;

    TdJsonTransport(const TdJsonTransport&) = delete;
    TdJsonTransport& operator=(const TdJsonTransport&) = delete;

    /// Attempts to load tdjson. Returns true when all required exports exist.
    bool load(QString* error = nullptr);
    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] QString loadError() const;

    /// Creates the TDLib client and starts the receiver thread.
    bool start(QString* error = nullptr);
    void stop();
    [[nodiscard]] bool isRunning() const;

    /// Sends one request. Ignored (and reported through transportError) when
    /// the client is not running.
    void send(const QJsonObject& request);

    /// Executes a request synchronously in TDLib. Only for local operations
    /// such as option validation; never use it for network requests.
    [[nodiscard]] Result<QJsonObject> execute(const QJsonObject& request);

signals:
    void updateReceived(const QJsonObject& update);
    void transportError(const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace beygram::telegram
