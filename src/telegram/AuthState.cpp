#include "AuthState.h"

namespace beygram::telegram {

QString authStageName(AuthStage stage)
{
    switch (stage) {
    case AuthStage::Unavailable: return QStringLiteral("unavailable");
    case AuthStage::NotConfigured: return QStringLiteral("not_configured");
    case AuthStage::Idle: return QStringLiteral("idle");
    case AuthStage::Loading: return QStringLiteral("loading");
    case AuthStage::WaitTdlibParameters: return QStringLiteral("wait_tdlib_parameters");
    case AuthStage::WaitPhoneNumber: return QStringLiteral("wait_phone_number");
    case AuthStage::WaitCode: return QStringLiteral("wait_code");
    case AuthStage::WaitPassword: return QStringLiteral("wait_password");
    case AuthStage::WaitRegistration: return QStringLiteral("wait_registration");
    case AuthStage::WaitQrCode: return QStringLiteral("wait_qr_code");
    case AuthStage::Ready: return QStringLiteral("ready");
    case AuthStage::Closing: return QStringLiteral("closing");
    case AuthStage::Closed: return QStringLiteral("closed");
    case AuthStage::Error: return QStringLiteral("error");
    }
    return QStringLiteral("unknown");
}

AuthStage authStageFromTdlib(const QString& authorizationStateName)
{
    if (authorizationStateName == QLatin1String("authorizationStateWaitTdlibParameters"))
        return AuthStage::WaitTdlibParameters;
    if (authorizationStateName == QLatin1String("authorizationStateWaitPhoneNumber"))
        return AuthStage::WaitPhoneNumber;
    if (authorizationStateName == QLatin1String("authorizationStateWaitCode"))
        return AuthStage::WaitCode;
    if (authorizationStateName == QLatin1String("authorizationStateWaitPassword"))
        return AuthStage::WaitPassword;
    if (authorizationStateName == QLatin1String("authorizationStateWaitRegistration"))
        return AuthStage::WaitRegistration;
    if (authorizationStateName == QLatin1String("authorizationStateWaitQrCode"))
        return AuthStage::WaitQrCode;
    if (authorizationStateName == QLatin1String("authorizationStateReady"))
        return AuthStage::Ready;
    if (authorizationStateName == QLatin1String("authorizationStateLoggingOut"))
        return AuthStage::Closing;
    if (authorizationStateName == QLatin1String("authorizationStateClosing"))
        return AuthStage::Closing;
    if (authorizationStateName == QLatin1String("authorizationStateClosed"))
        return AuthStage::Closed;
    return AuthStage::Loading;
}

QString authStageTitle(AuthStage stage)
{
    switch (stage) {
    case AuthStage::Unavailable:
        return QStringLiteral("TDLib runtime is not available");
    case AuthStage::NotConfigured:
        return QStringLiteral("Telegram API credentials are missing");
    case AuthStage::Idle:
        return QStringLiteral("Telegram core is stopped");
    case AuthStage::Loading:
        return QStringLiteral("Starting the Telegram core");
    case AuthStage::WaitTdlibParameters:
        return QStringLiteral("Initializing TDLib");
    case AuthStage::WaitPhoneNumber:
        return QStringLiteral("Waiting for a phone number");
    case AuthStage::WaitCode:
        return QStringLiteral("Waiting for the login code");
    case AuthStage::WaitPassword:
        return QStringLiteral("Waiting for the two-step password");
    case AuthStage::WaitRegistration:
        return QStringLiteral("Waiting for account registration");
    case AuthStage::WaitQrCode:
        return QStringLiteral("Waiting for QR login confirmation");
    case AuthStage::Ready:
        return QStringLiteral("Telegram account is authorized");
    case AuthStage::Closing:
        return QStringLiteral("Closing the Telegram session");
    case AuthStage::Closed:
        return QStringLiteral("Telegram session is closed");
    case AuthStage::Error:
        return QStringLiteral("Telegram core stopped with an error");
    }
    return QStringLiteral("Unknown state");
}

bool authStageIsTerminalFailure(AuthStage stage)
{
    return stage == AuthStage::Unavailable || stage == AuthStage::NotConfigured
        || stage == AuthStage::Error;
}

} // namespace beygram::telegram
