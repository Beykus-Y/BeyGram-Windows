#pragma once

#include <QString>

namespace beygram::telegram {

/// UI-independent authorization stage, mirroring TDLib's authorization states.
/// The UI never sees TDLib's raw JSON.
enum class AuthStage {
    Unavailable,        ///< tdjson could not be loaded
    NotConfigured,      ///< api_id/api_hash are missing
    Locked,             ///< another process owns this TDLib database
    Idle,               ///< client not started
    Loading,            ///< client created, waiting for TDLib
    WaitTdlibParameters,
    WaitPhoneNumber,
    WaitCode,
    WaitPassword,
    WaitRegistration,
    WaitEmailAddress,
    WaitEmailCode,
    WaitQrCode,
    Ready,
    Closing,
    Closed,
    Error,
};

[[nodiscard]] QString authStageName(AuthStage stage);
[[nodiscard]] AuthStage authStageFromTdlib(const QString& authorizationStateName);
[[nodiscard]] QString authStageTitle(AuthStage stage);
[[nodiscard]] bool authStageIsTerminalFailure(AuthStage stage);

} // namespace beygram::telegram
