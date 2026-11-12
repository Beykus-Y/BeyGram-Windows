#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Server-side state of Telegram's two-step verification.
///
/// This projection carries no password and no code. `hint` is the hint Telegram
/// stores for the password, and `recoveryEmail` is the address (possibly a
/// masked pattern) that TDLib reports; both are display values, never
/// credentials. The struct is a plain value, so it can be cached per account and
/// copied across threads.
struct PasswordState {
    bool hasPassword = false;
    QString hint;
    bool hasRecoveryEmail = false;
    QString recoveryEmail;
    /// Attempts left before Telegram refuses the password; never negative.
    int remainingAttempts = 0;
    /// Unix timestamp after which another attempt is accepted; 0 when unknown.
    qint64 nextAttemptDate = 0;
};

/// Projects a TDLib passwordState answer onto PasswordState.
///
/// Reads has_password, password_hint, recovery_email_address,
/// remaining_attempts and next_attempt_date. hasRecoveryEmail follows a
/// non-empty recovery_email_address. Missing fields keep their defaults and a
/// negative attempt count is clamped to 0. Never logs and never throws.
[[nodiscard]] PasswordState passwordStateFromResponse(const QJsonObject& response);

/// Builds getPasswordState, whose answer passwordStateFromResponse projects.
[[nodiscard]] QJsonObject getPasswordStateRequest();

/// Builds checkPassword. The password is passed verbatim: surrounding spaces and
/// line breaks are part of it and are never trimmed.
[[nodiscard]] QJsonObject checkPasswordRequest(const QString& password);

/// Builds setPassword. Both passwords are passed verbatim. The hint is trimmed
/// and clamped to 64 characters. A recoveryEmail that trims and lower-cases to
/// an address containing '@' is sent with set_recovery_email_address=true;
/// otherwise the recovery address is left untouched. An empty new password
/// disables two-step verification, exactly as TDLib defines it.
[[nodiscard]] QJsonObject setPasswordRequest(const QString& oldPassword,
                                             const QString& newPassword,
                                             const QString& hint,
                                             const QString& recoveryEmail);

/// Builds removePassword. The password is passed verbatim.
[[nodiscard]] QJsonObject removePasswordRequest(const QString& password);

/// Builds recoverPassword. recoveryCode is a code sent to the recovery address,
/// not the password itself, so it is trimmed.
[[nodiscard]] QJsonObject recoverPasswordRequest(const QString& recoveryCode);

/// Builds getRecoveryEmailAddress, which also verifies a password. The password
/// is passed verbatim.
[[nodiscard]] QJsonObject getRecoveryEmailAddressRequest(const QString& password);

/// Builds setRecoveryEmailAddress. The password is passed verbatim. The email is
/// trimmed and lower-cased and must contain '@'; a value that does not becomes
/// an empty address, so a malformed address is never sent. The change stays
/// pending until Telegram confirms it.
[[nodiscard]] QJsonObject setRecoveryEmailAddressRequest(const QString& password,
                                                         const QString& email);

} // namespace beygram::telegram
