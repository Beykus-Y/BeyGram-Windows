#pragma once

#include "AuthState.h"
#include "TelegramConfig.h"
#include "TdJsonTransport.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace beygram::telegram {

/// One TDLib session, isolated from the UI. Future multi-account support
/// creates one instance per account; there is intentionally no singleton and
/// no global TDLib client.
class TelegramSession final : public QObject {
    Q_OBJECT

public:
    explicit TelegramSession(TelegramConfig config, QObject* parent = nullptr);
    ~TelegramSession() override;

    TelegramSession(const TelegramSession&) = delete;
    TelegramSession& operator=(const TelegramSession&) = delete;

    [[nodiscard]] const TelegramConfig& config() const noexcept { return m_config; }
    [[nodiscard]] AuthStage stage() const noexcept { return m_stage; }
    [[nodiscard]] QString stageDetail() const { return m_stageDetail; }
    [[nodiscard]] QString connectionStateName() const { return m_connectionState; }
    [[nodiscard]] QString connectionStateTitle() const;
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] bool isReady() const noexcept { return m_stage == AuthStage::Ready; }

    /// Loads tdjson without creating a client. Used by the UI to report the
    /// real runtime state before a session is started.
    [[nodiscard]] bool probeTdlib(QString* detail = nullptr);

    /// Starts the client. Reports Unavailable/NotConfigured instead of
    /// pretending to connect when prerequisites are missing.
    void start();

    /// Stops the receiver and destroys the TDLib client. This does not log the
    /// Telegram account out and keeps the local database.
    void stop();

    /// Sends TDLib `Close`. The closed update arrives asynchronously.
    void requestClose();

    void submitPhoneNumber(const QString& phoneNumber);
    void submitCode(const QString& code);
    void submitPassword(const QString& password);
    void requestQrCodeLogin();
    void resetError();

    /// Raw request entry point for the upcoming repositories. Never call this
    /// from QML; UI code uses domain-level controllers.
    void sendRequest(const QJsonObject& request);

signals:
    void stageChanged(AuthStage stage);
    void connectionStateChanged();
    void errorOccurred(const QString& message);
    void updateReceived(const QJsonObject& update);

private:
    void handleUpdate(const QJsonObject& update);
    void handleAuthorizationState(const QJsonObject& state);
    void setStage(AuthStage stage, const QString& detail = {});
    void sendTdlibParameters();
    void reportError(const QString& message);

    TelegramConfig m_config;
    TdJsonTransport m_transport;
    AuthStage m_stage = AuthStage::Idle;
    QString m_stageDetail;
    QString m_connectionState = QStringLiteral("unknown");
    QString m_lastError;
    QString m_qrLink; // never exposed or logged
};

} // namespace beygram::telegram

Q_DECLARE_METATYPE(beygram::telegram::AuthStage)
