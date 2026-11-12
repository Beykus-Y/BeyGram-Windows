#include "AccountSecurity.h"

#include "TelegramTypes.h"

#include <algorithm>
#include <limits>

namespace beygram::telegram {
namespace {

constexpr int kMaxPasswordHintLength = 64;

/// Cuts text to at most maxLength Unicode code points without splitting a
/// surrogate pair, which would leave half an emoji in the hint.
QString limitedToCodePoints(const QString& text, int maxLength)
{
    int codePoints = 0;
    int index = 0;
    while (index < text.size()) {
        if (codePoints == maxLength)
            return text.left(index);

        const QChar character = text.at(index);
        const bool surrogatePair = character.isHighSurrogate() && index + 1 < text.size()
            && text.at(index + 1).isLowSurrogate();
        index += surrogatePair ? 2 : 1;
        ++codePoints;
    }
    return text;
}

// TDLib takes the recovery address trimmed; lower case makes the stored value
// stable regardless of how the user typed it. An address without '@' cannot be
// an email, so it normalises to empty and is never sent.
QString normalizedRecoveryEmail(const QString& email)
{
    const QString normalized = email.trimmed().toLower();
    return normalized.contains(QLatin1Char('@')) ? normalized : QString();
}

} // namespace

PasswordState passwordStateFromResponse(const QJsonObject& response)
{
    PasswordState state;
    state.hasPassword = response.value(QStringLiteral("has_password")).toBool();
    state.hint = response.value(QStringLiteral("password_hint")).toString();
    state.recoveryEmail =
        response.value(QStringLiteral("recovery_email_address")).toString();
    // The projection has a single recovery-email fact: a non-empty address.
    state.hasRecoveryEmail = !state.recoveryEmail.isEmpty();

    const qint64 attempts =
        jsonInt64(response.value(QStringLiteral("remaining_attempts")));
    state.remainingAttempts = static_cast<int>(
        std::clamp<qint64>(attempts, 0, std::numeric_limits<int>::max()));
    state.nextAttemptDate =
        jsonInt64(response.value(QStringLiteral("next_attempt_date")));
    return state;
}

QJsonObject getPasswordStateRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getPasswordState")},
    };
}

QJsonObject checkPasswordRequest(const QString& password)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("checkPassword")},
        {QStringLiteral("password"), password},
    };
}

QJsonObject setPasswordRequest(const QString& oldPassword, const QString& newPassword,
                               const QString& hint, const QString& recoveryEmail)
{
    const QString email = normalizedRecoveryEmail(recoveryEmail);
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setPassword")},
        {QStringLiteral("old_password"), oldPassword},
        {QStringLiteral("new_password"), newPassword},
        {QStringLiteral("new_hint"),
         limitedToCodePoints(hint.trimmed(), kMaxPasswordHintLength)},
        {QStringLiteral("set_recovery_email_address"), !email.isEmpty()},
        {QStringLiteral("new_recovery_email_address"), email},
    };
}

QJsonObject removePasswordRequest(const QString& password)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("removePassword")},
        {QStringLiteral("password"), password},
    };
}

QJsonObject recoverPasswordRequest(const QString& recoveryCode)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("recoverPassword")},
        {QStringLiteral("recovery_code"), recoveryCode.trimmed()},
    };
}

QJsonObject getRecoveryEmailAddressRequest(const QString& password)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getRecoveryEmailAddress")},
        {QStringLiteral("password"), password},
    };
}

QJsonObject setRecoveryEmailAddressRequest(const QString& password, const QString& email)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setRecoveryEmailAddress")},
        {QStringLiteral("password"), password},
        {QStringLiteral("new_recovery_email_address"),
         normalizedRecoveryEmail(email)},
    };
}

} // namespace beygram::telegram
