#include "telegram/AuthRequests.h"

#include <QtTest>

using namespace beygram::telegram::auth;

class AuthRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void normalizesPhoneNumbers();
    void rejectsTooShortOrTooLongPhoneNumbers();
    void buildsPhoneRequestWithoutSecretsLeakingToSettings();
    void buildsCodeAndPasswordRequests();
    void buildsEmailCodeRequest();
    void buildsRegistrationRequest();
    void buildsResendAndQrRequests();
    void describesKnownErrors();
    void preservesUnknownErrors();
};

void AuthRequestsTest::normalizesPhoneNumbers()
{
    QCOMPARE(normalizePhoneNumber(QStringLiteral("+1 (415) 555-0123")),
             QStringLiteral("+14155550123"));
    QCOMPARE(normalizePhoneNumber(QStringLiteral("0044 20 7946 0958")),
             QStringLiteral("+442079460958"));
    QCOMPARE(normalizePhoneNumber(QStringLiteral("  +7 999 123 45 67 ")),
             QStringLiteral("+79991234567"));
}

void AuthRequestsTest::rejectsTooShortOrTooLongPhoneNumbers()
{
    QVERIFY(normalizePhoneNumber(QStringLiteral("123")).isEmpty());
    QVERIFY(normalizePhoneNumber(QStringLiteral("+1234567890123456")).isEmpty());
    QVERIFY(normalizePhoneNumber(QString()).isEmpty());
}

void AuthRequestsTest::buildsPhoneRequestWithoutSecretsLeakingToSettings()
{
    const QJsonObject request = setPhoneNumberRequest(QStringLiteral("+14155550123"));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setAuthenticationPhoneNumber"));
    QCOMPARE(request.value(QStringLiteral("phone_number")).toString(),
             QStringLiteral("+14155550123"));
    QVERIFY(request.value(QStringLiteral("settings")).isNull());
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void AuthRequestsTest::buildsCodeAndPasswordRequests()
{
    const QJsonObject code = checkCodeRequest(QStringLiteral(" 12 34 5 "));
    QCOMPARE(code.value(QStringLiteral("@type")).toString(),
             QStringLiteral("checkAuthenticationCode"));
    QCOMPARE(code.value(QStringLiteral("code")).toString(), QStringLiteral("12345"));

    const QJsonObject password = checkPasswordRequest(QStringLiteral(" pass phrase "));
    QCOMPARE(password.value(QStringLiteral("@type")).toString(),
             QStringLiteral("checkAuthenticationPassword"));
    QCOMPARE(password.value(QStringLiteral("password")).toString(),
             QStringLiteral(" pass phrase "));
}

void AuthRequestsTest::buildsEmailCodeRequest()
{
    const QJsonObject request = checkEmailCodeRequest(QStringLiteral("987 654"));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("checkAuthenticationEmailCode"));
    const QJsonObject code = request.value(QStringLiteral("code")).toObject();
    QCOMPARE(code.value(QStringLiteral("@type")).toString(),
             QStringLiteral("emailAddressAuthenticationCode"));
    QCOMPARE(code.value(QStringLiteral("code")).toString(), QStringLiteral("987654"));
}

void AuthRequestsTest::buildsRegistrationRequest()
{
    const QJsonObject request =
        registerUserRequest(QStringLiteral(" Ada "), QStringLiteral(" Lovelace "));
    QCOMPARE(request.value(QStringLiteral("first_name")).toString(), QStringLiteral("Ada"));
    QCOMPARE(request.value(QStringLiteral("last_name")).toString(), QStringLiteral("Lovelace"));
}

void AuthRequestsTest::buildsResendAndQrRequests()
{
    QCOMPARE(resendCodeRequest().value(QStringLiteral("@type")).toString(),
             QStringLiteral("resendAuthenticationCode"));
    QCOMPARE(requestQrCodeRequest().value(QStringLiteral("@type")).toString(),
             QStringLiteral("requestQrCodeAuthentication"));
}

void AuthRequestsTest::describesKnownErrors()
{
    QVERIFY(describeError(400, QStringLiteral("PHONE_NUMBER_INVALID"))
                .contains(QStringLiteral("international format")));
    QVERIFY(describeError(400, QStringLiteral("PHONE_CODE_INVALID"))
                .contains(QStringLiteral("incorrect")));
    QVERIFY(describeError(400, QStringLiteral("PASSWORD_HASH_INVALID"))
                .contains(QStringLiteral("password")));
    QVERIFY(describeError(420, QStringLiteral("FLOOD_WAIT_42"))
                .contains(QStringLiteral("42")));
    QVERIFY(describeError(400, QStringLiteral("EMAIL_CODE_EXPIRED"))
                .contains(QStringLiteral("expired")));
}

void AuthRequestsTest::preservesUnknownErrors()
{
    QCOMPARE(describeError(500, QStringLiteral("Something unexpected")),
             QStringLiteral("Something unexpected"));
    QVERIFY(describeError(500, QString()).contains(QStringLiteral("500")));
}

QTEST_GUILESS_MAIN(AuthRequestsTest)
#include "tst_auth_requests.moc"
