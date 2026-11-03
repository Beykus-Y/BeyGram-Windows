#include "AuthRequests.h"

#include <QJsonArray>
#include <QJsonValue>

namespace beygram::telegram::auth {

QString normalizePhoneNumber(const QString& input)
{
    QString value = input.trimmed();
    if (value.startsWith(QStringLiteral("00")))
        value = QStringLiteral("+") + value.mid(2);

    QString digits;
    digits.reserve(value.size());
    for (const QChar character : value) {
        if (character.isDigit())
            digits.append(character);
    }
    if (digits.size() < 7 || digits.size() > 15)
        return {};
    return QStringLiteral("+") + digits;
}

QString normalizeCode(const QString& input)
{
    QString value;
    for (const QChar character : input.trimmed()) {
        if (character.isDigit())
            value.append(character);
    }
    return value;
}

QJsonObject setPhoneNumberRequest(const QString& normalizedPhone)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setAuthenticationPhoneNumber")},
        {QStringLiteral("phone_number"), normalizedPhone},
        // TDLib explicitly supports null to use default authentication settings.
        {QStringLiteral("settings"), QJsonValue::Null},
    };
}

QJsonObject checkCodeRequest(const QString& code)
{
    return QJsonObject{{QStringLiteral("@type"), QStringLiteral("checkAuthenticationCode")},
                       {QStringLiteral("code"), normalizeCode(code)}};
}

QJsonObject checkPasswordRequest(const QString& password)
{
    return QJsonObject{{QStringLiteral("@type"), QStringLiteral("checkAuthenticationPassword")},
                       {QStringLiteral("password"), password}};
}

QJsonObject setEmailAddressRequest(const QString& email)
{
    return QJsonObject{{QStringLiteral("@type"), QStringLiteral("setAuthenticationEmailAddress")},
                       {QStringLiteral("email_address"), email.trimmed()}};
}

QJsonObject checkEmailCodeRequest(const QString& code)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("checkAuthenticationEmailCode")},
        {QStringLiteral("code"),
         QJsonObject{{QStringLiteral("@type"), QStringLiteral("emailAddressAuthenticationCode")},
                     {QStringLiteral("code"), normalizeCode(code)}}},
    };
}

QJsonObject registerUserRequest(const QString& firstName, const QString& lastName)
{
    return QJsonObject{{QStringLiteral("@type"), QStringLiteral("registerUser")},
                       {QStringLiteral("first_name"), firstName.trimmed()},
                       {QStringLiteral("last_name"), lastName.trimmed()},
                       {QStringLiteral("disable_notification"), false}};
}

QJsonObject resendCodeRequest()
{
    return QJsonObject{{QStringLiteral("@type"), QStringLiteral("resendAuthenticationCode")},
                       {QStringLiteral("reason"), QJsonValue::Null}};
}

QJsonObject requestQrCodeRequest()
{
    return QJsonObject{{QStringLiteral("@type"), QStringLiteral("requestQrCodeAuthentication")},
                       {QStringLiteral("other_user_ids"), QJsonArray()}};
}

QString describeError(int code, const QString& rawMessage)
{
    const QString raw = rawMessage;
    if (raw == QLatin1String("PHONE_NUMBER_INVALID"))
        return QStringLiteral("That phone number is not valid. Use international format "
                              "starting with + and the country code.");
    if (raw == QLatin1String("PHONE_CODE_INVALID") || raw == QLatin1String("PHONE_CODE_EMPTY"))
        return QStringLiteral("The login code is incorrect.");
    if (raw == QLatin1String("PHONE_CODE_EXPIRED"))
        return QStringLiteral("The login code has expired. Request a new one.");
    if (raw == QLatin1String("PASSWORD_HASH_INVALID"))
        return QStringLiteral("The two-step verification password is incorrect.");
    if (raw == QLatin1String("PHONE_NUMBER_BANNED"))
        return QStringLiteral("This phone number is banned from Telegram.");
    if (raw == QLatin1String("PHONE_NUMBER_UNOCCUPIED"))
        return QStringLiteral("This phone number is not registered.");
    if (raw == QLatin1String("EMAIL_ADDRESS_INVALID"))
        return QStringLiteral("That email address is not valid.");
    if (raw == QLatin1String("EMAIL_CODE_INVALID") || raw == QLatin1String("EMAIL_CODE_EMPTY"))
        return QStringLiteral("The email code is incorrect.");
    if (raw == QLatin1String("EMAIL_CODE_EXPIRED"))
        return QStringLiteral("The email code has expired. Request a new one.");
    if (raw == QLatin1String("FIRSTNAME_INVALID"))
        return QStringLiteral("The first name is not valid.");
    if (raw.startsWith(QLatin1String("FLOOD_WAIT_"))) {
        const QString seconds = raw.mid(QStringLiteral("FLOOD_WAIT_").size());
        return QStringLiteral("Too many attempts. Wait %1 seconds and try again.").arg(seconds);
    }
    if (raw.isEmpty())
        return QStringLiteral("Telegram returned an error (code %1).").arg(code);
    return raw;
}

} // namespace beygram::telegram::auth
