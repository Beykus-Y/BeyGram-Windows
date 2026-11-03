#pragma once

#include <QJsonObject>
#include <QString>

namespace beygram::telegram::auth {

/// Normalises a user-entered phone number to the international `+digits` form
/// TDLib expects. Returns an empty string when the input is clearly invalid.
[[nodiscard]] QString normalizePhoneNumber(const QString& input);

/// Removes spaces and separators from an authentication code.
[[nodiscard]] QString normalizeCode(const QString& input);

// Request builders. They contain no logging and no @extra; the transport adds
// the correlation id.
[[nodiscard]] QJsonObject setPhoneNumberRequest(const QString& normalizedPhone);
[[nodiscard]] QJsonObject checkCodeRequest(const QString& code);
[[nodiscard]] QJsonObject checkPasswordRequest(const QString& password);
[[nodiscard]] QJsonObject setEmailAddressRequest(const QString& email);
[[nodiscard]] QJsonObject checkEmailCodeRequest(const QString& code);
[[nodiscard]] QJsonObject registerUserRequest(const QString& firstName,
                                              const QString& lastName);
[[nodiscard]] QJsonObject resendCodeRequest();
[[nodiscard]] QJsonObject requestQrCodeRequest();

/// Turns TDLib's error message/code into a user-facing sentence.
[[nodiscard]] QString describeError(int code, const QString& rawMessage);

} // namespace beygram::telegram::auth
