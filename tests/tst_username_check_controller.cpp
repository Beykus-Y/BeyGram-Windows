#include "telegram/UsernameCheckController.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using beygram::telegram::UsernameCheckController;

namespace {

using Status = UsernameCheckController::Status;

/// A canonical username of exactly the requested length.
QString filled(int length)
{
    return QString(length, QLatin1Char('a'));
}

} // namespace

class UsernameCheckControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsIdle();
    void acceptsCanonicalInput();
    void acceptsUnderscoreAndDigits();
    void stripsAtSign();
    void trimsSurroundingWhitespace();
    void lowercasesInput();
    void blankInputIsIdle();
    void rejectsMalformedInput();
    void enforcesLengthBounds();
    void validFormatAcceptsCanonicalNames();
    void validFormatRejectsDecorationAndBadCharacters();
    void resultMakesAvailable();
    void resultMakesTaken();
    void resultIgnoredUnlessChecking();
    void sameInputKeepsPendingCheck();
    void sameInputIgnoresCaseAndDecoration();
    void editingRestartsCheck();
    void changedOnlyOnRealChanges();
    void lateResultAfterClearIsIgnored();
    void resetClearsState();
    void resetFromIdleIsSilent();
    void acceptsNewCheckAfterReset();
};

void UsernameCheckControllerTest::startsIdle()
{
    UsernameCheckController controller;

    QVERIFY(controller.input().isEmpty());
    QCOMPARE(controller.status(), Status::Idle);
    QVERIFY(!controller.canSubmit());
}

void UsernameCheckControllerTest::acceptsCanonicalInput()
{
    UsernameCheckController controller;

    controller.setInput(QStringLiteral("alice_2024"));

    QCOMPARE(controller.input(), QStringLiteral("alice_2024"));
    // Well-formed text starts a check; it is not a result yet.
    QCOMPARE(controller.status(), Status::Checking);
    QVERIFY(!controller.canSubmit());
}

void UsernameCheckControllerTest::acceptsUnderscoreAndDigits()
{
    UsernameCheckController controller;

    controller.setInput(QStringLiteral("a_b_1"));
    QCOMPARE(controller.status(), Status::Checking);

    controller.setInput(QStringLiteral("z9_9_9"));
    QCOMPARE(controller.status(), Status::Checking);
}

void UsernameCheckControllerTest::stripsAtSign()
{
    UsernameCheckController controller;

    controller.setInput(QStringLiteral("@alice_2024"));

    QCOMPARE(controller.input(), QStringLiteral("alice_2024"));
    QCOMPARE(controller.status(), Status::Checking);
}

void UsernameCheckControllerTest::trimsSurroundingWhitespace()
{
    UsernameCheckController controller;

    controller.setInput(QStringLiteral("  @alice_2024 \t\n"));

    QCOMPARE(controller.input(), QStringLiteral("alice_2024"));
    QCOMPARE(controller.status(), Status::Checking);
}

void UsernameCheckControllerTest::lowercasesInput()
{
    UsernameCheckController controller;

    controller.setInput(QStringLiteral("Alice_2024"));

    QCOMPARE(controller.input(), QStringLiteral("alice_2024"));
    QCOMPARE(controller.status(), Status::Checking);
}

void UsernameCheckControllerTest::blankInputIsIdle()
{
    UsernameCheckController controller;
    controller.setInput(QStringLiteral("alice_2024"));
    QCOMPARE(controller.status(), Status::Checking);

    // Deleting the text cancels the check instead of leaving a stale answer.
    controller.setInput(QString());
    QVERIFY(controller.input().isEmpty());
    QCOMPARE(controller.status(), Status::Idle);
    QVERIFY(!controller.canSubmit());

    controller.setInput(QStringLiteral("   "));
    QCOMPARE(controller.status(), Status::Idle);

    // A lone '@' is decoration around nothing.
    controller.setInput(QStringLiteral("@"));
    QVERIFY(controller.input().isEmpty());
    QCOMPARE(controller.status(), Status::Idle);
}

void UsernameCheckControllerTest::rejectsMalformedInput()
{
    const QStringList malformed{
        QStringLiteral("abcd"),          // one character short
        QStringLiteral("1alice"),        // starts with a digit
        QStringLiteral("_alice"),        // starts with an underscore
        QStringLiteral("alice-bob"),     // hyphen
        QStringLiteral("alice bob"),     // space
        QStringLiteral("alice.bob"),     // dot
        QString(5, QChar(u'\u0430')),     // not ASCII (five Cyrillic 'a')
        filled(33),                      // one character too long
    };

    for (const QString& text : malformed) {
        UsernameCheckController controller;
        QSignalSpy spy(&controller, &UsernameCheckController::changed);

        controller.setInput(text);

        QCOMPARE(controller.input(), text);
        QCOMPARE(controller.status(), Status::Invalid);
        QVERIFY(!controller.canSubmit());
        QCOMPARE(spy.count(), 1);

        // Malformed text must never be sent, so an answer for it is ignored.
        controller.setResult(true);
        QCOMPARE(controller.status(), Status::Invalid);
        QVERIFY(!controller.canSubmit());
        QCOMPARE(spy.count(), 1);
    }
}

void UsernameCheckControllerTest::enforcesLengthBounds()
{
    UsernameCheckController controller;

    controller.setInput(filled(4));
    QCOMPARE(controller.status(), Status::Invalid);

    controller.setInput(filled(5));
    QCOMPARE(controller.status(), Status::Checking);

    controller.setInput(filled(32));
    QCOMPARE(controller.status(), Status::Checking);

    controller.setInput(filled(33));
    QCOMPARE(controller.status(), Status::Invalid);
}

void UsernameCheckControllerTest::validFormatAcceptsCanonicalNames()
{
    QVERIFY(UsernameCheckController::validFormat(QStringLiteral("abcde")));
    QVERIFY(UsernameCheckController::validFormat(QStringLiteral("alice_2024")));
    QVERIFY(UsernameCheckController::validFormat(QStringLiteral("a1234")));
    QVERIFY(UsernameCheckController::validFormat(filled(32)));
}

void UsernameCheckControllerTest::validFormatRejectsDecorationAndBadCharacters()
{
    QVERIFY(!UsernameCheckController::validFormat(QString()));
    QVERIFY(!UsernameCheckController::validFormat(QStringLiteral("abc")));
    QVERIFY(!UsernameCheckController::validFormat(QStringLiteral("abcd")));
    QVERIFY(!UsernameCheckController::validFormat(filled(33)));
    QVERIFY(!UsernameCheckController::validFormat(QStringLiteral("1alice")));
    QVERIFY(!UsernameCheckController::validFormat(QStringLiteral("_alice")));
    QVERIFY(!UsernameCheckController::validFormat(QStringLiteral("alice-bob")));
    QVERIFY(!UsernameCheckController::validFormat(QStringLiteral("alice bob")));

    // The predicate sees the canonical form only: setInput() strips the '@' and
    // lower cases before asking, a direct caller has to do the same.
    QVERIFY(!UsernameCheckController::validFormat(QStringLiteral("@alice")));
    QVERIFY(!UsernameCheckController::validFormat(QStringLiteral("Alice")));
}

void UsernameCheckControllerTest::resultMakesAvailable()
{
    UsernameCheckController controller;
    controller.setInput(QStringLiteral("alice_2024"));
    QSignalSpy spy(&controller, &UsernameCheckController::changed);

    controller.setResult(true);

    QCOMPARE(controller.status(), Status::Available);
    QVERIFY(controller.canSubmit());
    QCOMPARE(spy.count(), 1);
}

void UsernameCheckControllerTest::resultMakesTaken()
{
    UsernameCheckController controller;
    controller.setInput(QStringLiteral("alice_2024"));
    QSignalSpy spy(&controller, &UsernameCheckController::changed);

    controller.setResult(false);

    QCOMPARE(controller.status(), Status::Taken);
    QVERIFY(!controller.canSubmit());
    QCOMPARE(spy.count(), 1);
}

void UsernameCheckControllerTest::resultIgnoredUnlessChecking()
{
    UsernameCheckController idle;
    QSignalSpy idleSpy(&idle, &UsernameCheckController::changed);
    idle.setResult(true);
    QCOMPARE(idle.status(), Status::Idle);
    QCOMPARE(idleSpy.count(), 0);

    UsernameCheckController invalid;
    invalid.setInput(QStringLiteral("ab"));
    QSignalSpy invalidSpy(&invalid, &UsernameCheckController::changed);
    invalid.setResult(false);
    QCOMPARE(invalid.status(), Status::Invalid);
    QCOMPARE(invalidSpy.count(), 0);

    // A second answer cannot overturn the first one either.
    UsernameCheckController available;
    available.setInput(QStringLiteral("alice_2024"));
    available.setResult(true);
    QSignalSpy availableSpy(&available, &UsernameCheckController::changed);
    available.setResult(false);
    QCOMPARE(available.status(), Status::Available);
    QCOMPARE(availableSpy.count(), 0);

    UsernameCheckController taken;
    taken.setInput(QStringLiteral("alice_2024"));
    taken.setResult(false);
    QSignalSpy takenSpy(&taken, &UsernameCheckController::changed);
    taken.setResult(true);
    QCOMPARE(taken.status(), Status::Taken);
    QCOMPARE(takenSpy.count(), 0);
}

void UsernameCheckControllerTest::sameInputKeepsPendingCheck()
{
    UsernameCheckController controller;
    controller.setInput(QStringLiteral("alice_2024"));
    QSignalSpy spy(&controller, &UsernameCheckController::changed);

    // The text field may report the same text again; that is not a new check.
    controller.setInput(QStringLiteral("alice_2024"));

    QCOMPARE(controller.status(), Status::Checking);
    QCOMPARE(spy.count(), 0);

    // The request is still pending, so its answer is still accepted.
    controller.setResult(true);
    QCOMPARE(controller.status(), Status::Available);
    QCOMPARE(spy.count(), 1);
}

void UsernameCheckControllerTest::sameInputIgnoresCaseAndDecoration()
{
    UsernameCheckController controller;
    controller.setInput(QStringLiteral("alice_2024"));
    QSignalSpy spy(&controller, &UsernameCheckController::changed);

    controller.setInput(QStringLiteral(" @Alice_2024 "));

    QCOMPARE(controller.input(), QStringLiteral("alice_2024"));
    QCOMPARE(controller.status(), Status::Checking);
    QCOMPARE(spy.count(), 0);
}

void UsernameCheckControllerTest::editingRestartsCheck()
{
    UsernameCheckController controller;
    controller.setInput(QStringLiteral("alice_2024"));
    controller.setResult(true);
    QCOMPARE(controller.status(), Status::Available);

    controller.setInput(QStringLiteral("alice_2025"));

    // The old answer belongs to the old name and may not carry over.
    QCOMPARE(controller.input(), QStringLiteral("alice_2025"));
    QCOMPARE(controller.status(), Status::Checking);
    QVERIFY(!controller.canSubmit());

    controller.setResult(false);
    QCOMPARE(controller.status(), Status::Taken);
}

void UsernameCheckControllerTest::changedOnlyOnRealChanges()
{
    UsernameCheckController controller;
    QSignalSpy spy(&controller, &UsernameCheckController::changed);

    controller.setInput(QString());
    controller.setInput(QStringLiteral("   "));
    controller.setInput(QStringLiteral("@"));
    QCOMPARE(spy.count(), 0);

    controller.setInput(QStringLiteral("ab"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.status(), Status::Invalid);

    controller.setInput(QStringLiteral("ab"));
    QCOMPARE(spy.count(), 1);

    controller.setInput(QStringLiteral("alice"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(controller.status(), Status::Checking);

    controller.setResult(true);
    QCOMPARE(spy.count(), 3);
    controller.setResult(true);
    QCOMPARE(spy.count(), 3);

    controller.setInput(QStringLiteral("alice"));
    QCOMPARE(spy.count(), 3);

    controller.reset();
    QCOMPARE(spy.count(), 4);
    controller.reset();
    QCOMPARE(spy.count(), 4);
}

void UsernameCheckControllerTest::lateResultAfterClearIsIgnored()
{
    UsernameCheckController controller;
    controller.setInput(QStringLiteral("alice_2024"));
    controller.setInput(QString());

    QSignalSpy spy(&controller, &UsernameCheckController::changed);
    controller.setResult(true);

    QCOMPARE(controller.status(), Status::Idle);
    QVERIFY(!controller.canSubmit());
    QCOMPARE(spy.count(), 0);
}

void UsernameCheckControllerTest::resetClearsState()
{
    UsernameCheckController controller;
    controller.setInput(QStringLiteral("alice_2024"));
    controller.setResult(false);
    QCOMPARE(controller.status(), Status::Taken);

    QSignalSpy spy(&controller, &UsernameCheckController::changed);
    controller.reset();

    QVERIFY(controller.input().isEmpty());
    QCOMPARE(controller.status(), Status::Idle);
    QVERIFY(!controller.canSubmit());
    QCOMPARE(spy.count(), 1);
}

void UsernameCheckControllerTest::resetFromIdleIsSilent()
{
    UsernameCheckController controller;
    QSignalSpy spy(&controller, &UsernameCheckController::changed);

    controller.reset();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.status(), Status::Idle);
}

void UsernameCheckControllerTest::acceptsNewCheckAfterReset()
{
    UsernameCheckController controller;
    controller.setInput(QStringLiteral("alice_2024"));
    controller.setResult(true);
    QCOMPARE(controller.status(), Status::Available);

    controller.reset();
    QSignalSpy spy(&controller, &UsernameCheckController::changed);

    // reset() is the way to ask for a fresh check of a name that already has an
    // answer, so the same text has to start over.
    controller.setInput(QStringLiteral("alice_2024"));

    QCOMPARE(controller.status(), Status::Checking);
    QVERIFY(!controller.canSubmit());
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(UsernameCheckControllerTest)
#include "tst_username_check_controller.moc"
