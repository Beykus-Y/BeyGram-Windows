#include "telegram/AccountSecurity.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

QJsonObject roundTrip(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

// A password the user could really type: spaces are meaningful and must survive
// every builder unchanged.
QString passwordWithSurroundings()
{
    return QStringLiteral("  correct horse\n");
}

} // namespace

class AccountSecurityTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesPasswordState();
    void parsesPartialAndEmptyResponses();
    void clampsRemainingAttempts();
    void derivesRecoveryFlagFromAddress();
    void buildsGetPasswordStateRequest();
    void passesPasswordsVerbatim();
    void buildsSetPasswordRequest();
    void leavesRecoveryEmailUntouchedWithoutOne();
    void rejectsMalformedRecoveryEmail();
    void buildsRecoveryEmailRequests();
    void buildsRemoveAndRecoverRequests();
    void requestsSurviveSerialization();
};

void AccountSecurityTest::parsesPasswordState()
{
    const QJsonObject response = parseObject(R"({
        "@type": "passwordState",
        "has_password": true,
        "password_hint": "first pet",
        "recovery_email_address": "u***@example.com",
        "remaining_attempts": 3,
        "next_attempt_date": 1700000000
    })");

    const PasswordState state = passwordStateFromResponse(response);
    QVERIFY(state.hasPassword);
    QCOMPARE(state.hint, QStringLiteral("first pet"));
    QVERIFY(state.hasRecoveryEmail);
    QCOMPARE(state.recoveryEmail, QStringLiteral("u***@example.com"));
    QCOMPARE(state.remainingAttempts, 3);
    QCOMPARE(state.nextAttemptDate, qint64(1700000000));
}

void AccountSecurityTest::parsesPartialAndEmptyResponses()
{
    const PasswordState empty = passwordStateFromResponse(QJsonObject{});
    QVERIFY(!empty.hasPassword);
    QVERIFY(empty.hint.isEmpty());
    QVERIFY(!empty.hasRecoveryEmail);
    QVERIFY(empty.recoveryEmail.isEmpty());
    QCOMPARE(empty.remainingAttempts, 0);
    QCOMPARE(empty.nextAttemptDate, qint64(0));

    // int64 timestamps arrive as strings on some transports, and the hint is a
    // display value that keeps its own spacing.
    const PasswordState partial = passwordStateFromResponse(parseObject(R"({
        "has_password": true,
        "password_hint": "  spaced  ",
        "remaining_attempts": 2,
        "next_attempt_date": "1700000123"
    })"));
    QVERIFY(partial.hasPassword);
    QCOMPARE(partial.hint, QStringLiteral("  spaced  "));
    QCOMPARE(partial.remainingAttempts, 2);
    QCOMPARE(partial.nextAttemptDate, qint64(1700000123));
}

void AccountSecurityTest::clampsRemainingAttempts()
{
    QCOMPARE(passwordStateFromResponse(
                 parseObject(R"({"remaining_attempts": -4})"))
                 .remainingAttempts,
             0);
    QCOMPARE(passwordStateFromResponse(
                 parseObject(R"({"remaining_attempts": 0})"))
                 .remainingAttempts,
             0);
    QCOMPARE(passwordStateFromResponse(
                 parseObject(R"({"remaining_attempts": 7})"))
                 .remainingAttempts,
             7);
}

void AccountSecurityTest::derivesRecoveryFlagFromAddress()
{
    const PasswordState addressOnly = passwordStateFromResponse(
        parseObject(R"({"recovery_email_address": "user@example.com"})"));
    QVERIFY(addressOnly.hasRecoveryEmail);

    const PasswordState masked = passwordStateFromResponse(
        parseObject(R"({"recovery_email_address": "u***@example.com"})"));
    QVERIFY(masked.hasRecoveryEmail);

    const PasswordState noAddress = passwordStateFromResponse(
        parseObject(R"({"recovery_email_address": ""})"));
    QVERIFY(!noAddress.hasRecoveryEmail);

    const PasswordState absent =
        passwordStateFromResponse(parseObject(R"({"has_password": false})"));
    QVERIFY(!absent.hasRecoveryEmail);
}

void AccountSecurityTest::buildsGetPasswordStateRequest()
{
    const QJsonObject request = getPasswordStateRequest();
    QCOMPARE(request.size(), 1);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getPasswordState"));
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void AccountSecurityTest::passesPasswordsVerbatim()
{
    const QString password = passwordWithSurroundings();

    const QJsonObject check = checkPasswordRequest(password);
    QCOMPARE(check.value(QStringLiteral("@type")).toString(),
             QStringLiteral("checkPassword"));
    QCOMPARE(check.value(QStringLiteral("password")).toString(), password);

    const QJsonObject remove = removePasswordRequest(password);
    QCOMPARE(remove.value(QStringLiteral("@type")).toString(),
             QStringLiteral("removePassword"));
    QCOMPARE(remove.value(QStringLiteral("password")).toString(), password);

    const QJsonObject lookup = getRecoveryEmailAddressRequest(password);
    QCOMPARE(lookup.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getRecoveryEmailAddress"));
    QCOMPARE(lookup.value(QStringLiteral("password")).toString(), password);

    const QJsonObject change =
        setPasswordRequest(password, password, QString(), QString());
    QCOMPARE(change.value(QStringLiteral("old_password")).toString(), password);
    QCOMPARE(change.value(QStringLiteral("new_password")).toString(), password);
}

void AccountSecurityTest::buildsSetPasswordRequest()
{
    const QJsonObject request = setPasswordRequest(
        QStringLiteral(" old "), QStringLiteral(" new "),
        QStringLiteral("  first pet  "), QStringLiteral("  User@Example.COM  "));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setPassword"));
    QCOMPARE(request.value(QStringLiteral("old_password")).toString(),
             QStringLiteral(" old "));
    QCOMPARE(request.value(QStringLiteral("new_password")).toString(),
             QStringLiteral(" new "));
    QCOMPARE(request.value(QStringLiteral("new_hint")).toString(),
             QStringLiteral("first pet"));
    QCOMPARE(request.value(QStringLiteral("set_recovery_email_address")).toBool(),
             true);
    QCOMPARE(request.value(QStringLiteral("new_recovery_email_address")).toString(),
             QStringLiteral("user@example.com"));

    const QString longHint = QStringLiteral("  ")
        + QString(80, QLatin1Char('h')) + QStringLiteral("  ");
    QCOMPARE(setPasswordRequest(QString(), QString(), longHint, QString())
                 .value(QStringLiteral("new_hint"))
                 .toString(),
             QString(64, QLatin1Char('h')));
}

void AccountSecurityTest::leavesRecoveryEmailUntouchedWithoutOne()
{
    const QJsonObject request = setPasswordRequest(
        QStringLiteral("old"), QStringLiteral("new"), QString(),
        QStringLiteral("   "));

    QCOMPARE(request.value(QStringLiteral("set_recovery_email_address")).toBool(),
             false);
    QVERIFY(request.value(QStringLiteral("new_recovery_email_address"))
                .toString()
                .isEmpty());
    // The field is always present so TDLib never has to guess.
    QVERIFY(request.contains(QStringLiteral("new_recovery_email_address")));
}

void AccountSecurityTest::rejectsMalformedRecoveryEmail()
{
    const QJsonObject change = setPasswordRequest(
        QStringLiteral("old"), QStringLiteral("new"), QString(),
        QStringLiteral("  user.example.com  "));
    QCOMPARE(change.value(QStringLiteral("set_recovery_email_address")).toBool(),
             false);
    QVERIFY(change.value(QStringLiteral("new_recovery_email_address"))
                .toString()
                .isEmpty());

    const QJsonObject set = setRecoveryEmailAddressRequest(
        QStringLiteral("old"), QStringLiteral("  user.example.com  "));
    QVERIFY(set.value(QStringLiteral("new_recovery_email_address"))
                .toString()
                .isEmpty());

    // The local check is deliberately minimal; Telegram validates the domain.
    QCOMPARE(setRecoveryEmailAddressRequest(QStringLiteral("old"),
                                            QStringLiteral(" A@B "))
                 .value(QStringLiteral("new_recovery_email_address"))
                 .toString(),
             QStringLiteral("a@b"));
}

void AccountSecurityTest::buildsRecoveryEmailRequests()
{
    const QJsonObject lookup = getRecoveryEmailAddressRequest(QStringLiteral(" secret "));
    QCOMPARE(lookup.size(), 2);
    QCOMPARE(lookup.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getRecoveryEmailAddress"));
    QCOMPARE(lookup.value(QStringLiteral("password")).toString(),
             QStringLiteral(" secret "));

    const QJsonObject change = setRecoveryEmailAddressRequest(
        QStringLiteral(" secret "), QStringLiteral("  New@Example.COM  "));
    QCOMPARE(change.size(), 3);
    QCOMPARE(change.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setRecoveryEmailAddress"));
    QCOMPARE(change.value(QStringLiteral("password")).toString(),
             QStringLiteral(" secret "));
    QCOMPARE(change.value(QStringLiteral("new_recovery_email_address")).toString(),
             QStringLiteral("new@example.com"));
    QVERIFY(!change.contains(QStringLiteral("@extra")));
}

void AccountSecurityTest::buildsRemoveAndRecoverRequests()
{
    const QJsonObject remove = removePasswordRequest(QStringLiteral(" pass "));
    QCOMPARE(remove.size(), 2);
    QCOMPARE(remove.value(QStringLiteral("@type")).toString(),
             QStringLiteral("removePassword"));
    QCOMPARE(remove.value(QStringLiteral("password")).toString(),
             QStringLiteral(" pass "));

    // The recovery code is a code from an email, so pasted padding is dropped.
    const QJsonObject recover = recoverPasswordRequest(QStringLiteral("  123 456  "));
    QCOMPARE(recover.size(), 2);
    QCOMPARE(recover.value(QStringLiteral("@type")).toString(),
             QStringLiteral("recoverPassword"));
    QCOMPARE(recover.value(QStringLiteral("recovery_code")).toString(),
             QStringLiteral("123 456"));
}

void AccountSecurityTest::requestsSurviveSerialization()
{
    const QJsonObject request = roundTrip(setPasswordRequest(
        QStringLiteral(" old "), QStringLiteral(" new "),
        QStringLiteral("  hint  "), QStringLiteral(" User@Example.COM ")));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setPassword"));
    QCOMPARE(request.value(QStringLiteral("old_password")).toString(),
             QStringLiteral(" old "));
    QCOMPARE(request.value(QStringLiteral("new_password")).toString(),
             QStringLiteral(" new "));
    QCOMPARE(request.value(QStringLiteral("new_hint")).toString(),
             QStringLiteral("hint"));
    QCOMPARE(request.value(QStringLiteral("set_recovery_email_address")).toBool(),
             true);
    QCOMPARE(request.value(QStringLiteral("new_recovery_email_address")).toString(),
             QStringLiteral("user@example.com"));

    const QJsonObject state = roundTrip(getPasswordStateRequest());
    QCOMPARE(state.size(), 1);
    QCOMPARE(state.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getPasswordState"));
}

QTEST_GUILESS_MAIN(AccountSecurityTest)
#include "tst_account_security.moc"
