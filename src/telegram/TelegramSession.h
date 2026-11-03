#pragma once

#include "AuthState.h"
#include "SessionLock.h"
#include "TelegramConfig.h"
#include "TdJsonTransport.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QTimer;

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
    [[nodiscard]] int lastErrorCode() const { return m_lastErrorCode; }
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] bool isReady() const noexcept { return m_stage == AuthStage::Ready; }
    [[nodiscard]] bool isLoggingOut() const noexcept { return m_loggingOut; }

    // Details of the current authorization screen. No secret is included:
    // QR links stay private to qrLoginLink().
    [[nodiscard]] int codeLength() const noexcept { return m_codeLength; }
    [[nodiscard]] QString codePhoneNumber() const { return m_codePhoneNumber; }
    [[nodiscard]] QString codeTypeName() const { return m_codeTypeName; }
    [[nodiscard]] int emailCodeLength() const noexcept { return m_emailCodeLength; }
    [[nodiscard]] QString emailPattern() const { return m_emailPattern; }
    [[nodiscard]] QString passwordHint() const { return m_passwordHint; }
    [[nodiscard]] bool passwordHasRecoveryEmail() const noexcept { return m_passwordHasRecoveryEmail; }
    [[nodiscard]] QString registrationTerms() const { return m_registrationTerms; }
    [[nodiscard]] int registrationMinUserAge() const noexcept { return m_registrationMinAge; }
    [[nodiscard]] int qrRevision() const noexcept { return m_qrRevision; }

    /// The tg:// QR login link. Secret: only the QR image provider should read
    /// it. Never log, store or expose this through a QML property.
    [[nodiscard]] QString qrLoginLink() const { return m_qrLink; }

    /// Loads tdjson without creating a client.
    [[nodiscard]] bool probeTdlib(QString* detail = nullptr);
    [[nodiscard]] QString tdlibPath() const;

    void start();

    /// Sends TDLib `Close` and waits for authorizationStateClosed. Returns true
    /// when the handshake completed. On timeout the client is aborted.
    bool shutdown(int timeoutMs = 5000);

    /// Stops immediately, without a close handshake. Keeps the local database.
    void abort();

    /// Sends TDLib `Close`; the closed update arrives asynchronously.
    void requestClose();

    /// Explicit, user-confirmed logout. Destroys local Telegram data on the
    /// TDLib side. Never call this automatically.
    void requestLogout();

    /// Closes and reopens the client, keeping the database. Used to return from
    /// a stuck code/password/QR state to the phone screen.
    void restartSession();

    void submitPhoneNumber(const QString& phoneNumber);
    void submitCode(const QString& code);
    void submitPassword(const QString& password);
    void submitEmailAddress(const QString& emailAddress);
    void submitEmailCode(const QString& code);
    void submitRegistration(const QString& firstName, const QString& lastName);
    void resendCode();
    void requestQrCodeLogin();

    void clearError();

    /// Raw request entry point for the upcoming repositories. Never call this
    /// from QML; UI code uses domain-level controllers.
    TdJsonTransport::RequestId sendRequest(const QJsonObject& request,
                                           TdJsonTransport::ResponseHandler handler = {},
                                           int timeoutMs = 30000);

signals:
    void stageChanged(AuthStage stage);
    void connectionStateChanged();
    void errorOccurred(const QString& message);
    void authRequestFinished();
    void qrCodeChanged();
    void updateReceived(const QJsonObject& update);

private:
    void handleUpdate(const QJsonObject& update);
    void handleAuthorizationState(const QJsonObject& state);
    void setStage(AuthStage stage, const QString& detail = {});
    void sendTdlibParameters();
    void sendAuthRequest(const QJsonObject& request, const QString& failureContext,
                         int timeoutMs = 120000);
    void reportError(const QString& message, int code = 0);
    void handleCloseTimeout();
    void releaseLock();
    void resetAuthDetails();
    void applyCodeInfo(const QJsonObject& codeInfo);
    [[nodiscard]] QString friendlyAuthError(const QJsonObject& error, int* code) const;

    TelegramConfig m_config;
    TdJsonTransport m_transport;
    SessionLock m_lock;
    QTimer* m_closeTimer = nullptr;

    AuthStage m_stage = AuthStage::Idle;
    QString m_stageDetail;
    QString m_connectionState = QStringLiteral("unknown");
    QString m_lastError;
    int m_lastErrorCode = 0;
    bool m_loggingOut = false;

    int m_codeLength = 0;
    QString m_codePhoneNumber;
    QString m_codeTypeName;
    int m_emailCodeLength = 0;
    QString m_emailPattern;
    QString m_passwordHint;
    bool m_passwordHasRecoveryEmail = false;
    QString m_registrationTerms;
    int m_registrationMinAge = 16;
    QString m_qrLink;
    int m_qrRevision = 0;
};

} // namespace beygram::telegram

Q_DECLARE_METATYPE(beygram::telegram::AuthStage)
