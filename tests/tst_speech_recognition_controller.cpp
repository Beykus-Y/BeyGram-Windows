#include "telegram/SpeechRecognitionController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class SpeechRecognitionControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsIdle();
    void startRejectsNonPositiveMessageId();
    void startRequestsRecognitionAndListens();
    void startDeliversTheRequestInRequestingState();
    void startResetsPreviousTextAndError();
    void ownerCanFailFromRecognitionRequested();
    void ownerCanCancelFromRecognitionRequested();
    void setPartialTextOnlyAcceptedWhileListening();
    void setPartialTextStoresVerbatimAndEmitsOnChange();
    void finishFromListeningStartsProcessing();
    void finishFromProcessingCompletes();
    void finishRejectsEmptyResult();
    void finishIgnoredOutsideListeningAndProcessing();
    void failOnlyWhileActive();
    void failClearsTextAndKeepsMessageId();
    void cancelResetsEverything();
    void stateNamesAreLowercase();
    void changedOnlyOnRealChanges();
};

void SpeechRecognitionControllerTest::startsIdle()
{
    const SpeechRecognitionController controller;

    QCOMPARE(controller.state(), SpeechRecognitionController::State::Idle);
    QCOMPARE(controller.stateName(), QStringLiteral("idle"));
    QCOMPARE(controller.messageId(), qint64(0));
    QVERIFY(controller.partialText().isEmpty());
    QVERIFY(controller.resultText().isEmpty());
    QVERIFY(controller.error().isEmpty());
    QVERIFY(!controller.active());
}

void SpeechRecognitionControllerTest::startRejectsNonPositiveMessageId()
{
    SpeechRecognitionController controller;
    QSignalSpy changedSpy(&controller, &SpeechRecognitionController::changed);
    QSignalSpy requestSpy(&controller, &SpeechRecognitionController::recognitionRequested);

    controller.start(0);
    controller.start(-7);

    QCOMPARE(controller.state(), SpeechRecognitionController::State::Idle);
    QCOMPARE(controller.messageId(), qint64(0));
    QVERIFY(!controller.active());
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);

    // A rejected id must not disturb an attempt that is already in flight.
    controller.start(3);
    changedSpy.clear();
    requestSpy.clear();

    controller.start(0);
    controller.start(-7);

    QCOMPARE(controller.state(), SpeechRecognitionController::State::Listening);
    QCOMPARE(controller.messageId(), qint64(3));
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
}

void SpeechRecognitionControllerTest::startRequestsRecognitionAndListens()
{
    SpeechRecognitionController controller;
    QSignalSpy changedSpy(&controller, &SpeechRecognitionController::changed);
    QSignalSpy requestSpy(&controller, &SpeechRecognitionController::recognitionRequested);

    controller.start(42);

    QCOMPARE(controller.state(), SpeechRecognitionController::State::Listening);
    QCOMPARE(controller.stateName(), QStringLiteral("listening"));
    QCOMPARE(controller.messageId(), qint64(42));
    QVERIFY(controller.active());
    QVERIFY(controller.partialText().isEmpty());
    QVERIFY(controller.resultText().isEmpty());
    QVERIFY(controller.error().isEmpty());
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.at(0).at(0).toLongLong(), qint64(42));
    QCOMPARE(changedSpy.count(), 1);
}

void SpeechRecognitionControllerTest::startDeliversTheRequestInRequestingState()
{
    SpeechRecognitionController controller;
    SpeechRecognitionController::State stateInside = SpeechRecognitionController::State::Idle;
    QString nameInside;
    qint64 idInside = 0;
    bool activeInside = false;

    connect(&controller, &SpeechRecognitionController::recognitionRequested, &controller,
        [&controller, &stateInside, &nameInside, &idInside, &activeInside](qint64 messageId) {
            stateInside = controller.state();
            nameInside = controller.stateName();
            idInside = messageId;
            activeInside = controller.active();
            // Partials describe speech that is still being captured, so the
            // request phase must not accept them.
            controller.setPartialText(QStringLiteral("too early"));
        });

    controller.start(9);

    QCOMPARE(stateInside, SpeechRecognitionController::State::Requesting);
    QCOMPARE(nameInside, QStringLiteral("requesting"));
    QCOMPARE(idInside, qint64(9));
    QVERIFY(activeInside);
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Listening);
    QVERIFY(controller.partialText().isEmpty());
}

void SpeechRecognitionControllerTest::startResetsPreviousTextAndError()
{
    SpeechRecognitionController controller;

    controller.start(1);
    controller.setPartialText(QStringLiteral("old partial"));
    controller.finish(QStringLiteral("old result"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Processing);
    QCOMPARE(controller.resultText(), QStringLiteral("old result"));

    controller.start(2);

    QCOMPARE(controller.state(), SpeechRecognitionController::State::Listening);
    QCOMPARE(controller.messageId(), qint64(2));
    QVERIFY(controller.partialText().isEmpty());
    QVERIFY(controller.resultText().isEmpty());
    QVERIFY(controller.error().isEmpty());

    controller.fail(QStringLiteral("boom"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Failed);
    QCOMPARE(controller.error(), QStringLiteral("boom"));

    controller.start(3);

    QCOMPARE(controller.state(), SpeechRecognitionController::State::Listening);
    QCOMPARE(controller.messageId(), qint64(3));
    QVERIFY(controller.error().isEmpty());
}

void SpeechRecognitionControllerTest::ownerCanFailFromRecognitionRequested()
{
    SpeechRecognitionController controller;
    QSignalSpy changedSpy(&controller, &SpeechRecognitionController::changed);
    QSignalSpy requestSpy(&controller, &SpeechRecognitionController::recognitionRequested);

    connect(&controller, &SpeechRecognitionController::recognitionRequested, &controller,
        [&controller](qint64) { controller.fail(QStringLiteral("no engine")); });

    controller.start(5);

    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Failed);
    QCOMPARE(controller.error(), QStringLiteral("no engine"));
    QCOMPARE(controller.messageId(), qint64(5));
    QVERIFY(!controller.active());
}

void SpeechRecognitionControllerTest::ownerCanCancelFromRecognitionRequested()
{
    SpeechRecognitionController controller;
    QSignalSpy changedSpy(&controller, &SpeechRecognitionController::changed);

    connect(&controller, &SpeechRecognitionController::recognitionRequested, &controller,
        [&controller](qint64) { controller.cancel(); });

    controller.start(6);

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Idle);
    QCOMPARE(controller.messageId(), qint64(0));
    QVERIFY(!controller.active());
}

void SpeechRecognitionControllerTest::setPartialTextOnlyAcceptedWhileListening()
{
    SpeechRecognitionController controller;
    QSignalSpy changedSpy(&controller, &SpeechRecognitionController::changed);

    controller.setPartialText(QStringLiteral("too early"));
    QVERIFY(controller.partialText().isEmpty());
    QCOMPARE(changedSpy.count(), 0);

    controller.start(7);
    QCOMPARE(changedSpy.count(), 1);

    controller.setPartialText(QStringLiteral("hello"));
    QCOMPARE(controller.partialText(), QStringLiteral("hello"));
    QCOMPARE(changedSpy.count(), 2);

    controller.finish(QStringLiteral("hello"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Processing);
    QCOMPARE(changedSpy.count(), 3);

    controller.setPartialText(QStringLiteral("too late"));
    QVERIFY(controller.partialText().isEmpty());
    QCOMPARE(changedSpy.count(), 3);

    controller.finish(QStringLiteral("final"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Done);
    QCOMPARE(changedSpy.count(), 4);

    controller.setPartialText(QStringLiteral("after done"));
    QVERIFY(controller.partialText().isEmpty());
    QCOMPARE(changedSpy.count(), 4);

    controller.start(8);
    controller.fail(QStringLiteral("boom"));
    QCOMPARE(changedSpy.count(), 6);

    controller.setPartialText(QStringLiteral("after failure"));
    QVERIFY(controller.partialText().isEmpty());
    QCOMPARE(changedSpy.count(), 6);
}

void SpeechRecognitionControllerTest::setPartialTextStoresVerbatimAndEmitsOnChange()
{
    SpeechRecognitionController controller;
    controller.start(10);
    QSignalSpy changedSpy(&controller, &SpeechRecognitionController::changed);

    controller.setPartialText(QStringLiteral("  hello  "));
    QCOMPARE(controller.partialText(), QStringLiteral("  hello  "));
    QCOMPARE(changedSpy.count(), 1);

    controller.setPartialText(QStringLiteral("  hello  "));
    QCOMPARE(changedSpy.count(), 1);

    // A revision may legitimately become empty again.
    controller.setPartialText(QString());
    QVERIFY(controller.partialText().isEmpty());
    QCOMPARE(changedSpy.count(), 2);

    controller.setPartialText(QString());
    QCOMPARE(changedSpy.count(), 2);
}

void SpeechRecognitionControllerTest::finishFromListeningStartsProcessing()
{
    SpeechRecognitionController controller;
    controller.start(11);
    controller.setPartialText(QStringLiteral("hello wor"));
    QSignalSpy changedSpy(&controller, &SpeechRecognitionController::changed);

    controller.finish(QStringLiteral("  hello world  "));

    QCOMPARE(controller.state(), SpeechRecognitionController::State::Processing);
    QCOMPARE(controller.stateName(), QStringLiteral("processing"));
    QCOMPARE(controller.messageId(), qint64(11));
    QCOMPARE(controller.resultText(), QStringLiteral("hello world"));
    QVERIFY(controller.partialText().isEmpty());
    QVERIFY(controller.error().isEmpty());
    QVERIFY(controller.active());
    QCOMPARE(changedSpy.count(), 1);
}

void SpeechRecognitionControllerTest::finishFromProcessingCompletes()
{
    SpeechRecognitionController controller;
    controller.start(12);
    controller.finish(QStringLiteral("first draft"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Processing);
    QCOMPARE(controller.resultText(), QStringLiteral("first draft"));

    QSignalSpy changedSpy(&controller, &SpeechRecognitionController::changed);
    controller.finish(QStringLiteral("\n final text \t"));

    QCOMPARE(controller.state(), SpeechRecognitionController::State::Done);
    QCOMPARE(controller.stateName(), QStringLiteral("done"));
    QCOMPARE(controller.messageId(), qint64(12));
    QCOMPARE(controller.resultText(), QStringLiteral("final text"));
    QVERIFY(controller.partialText().isEmpty());
    QVERIFY(controller.error().isEmpty());
    QVERIFY(!controller.active());
    QCOMPARE(changedSpy.count(), 1);

    // A late final result cannot change a completed attempt.
    controller.finish(QStringLiteral("later"));
    QCOMPARE(controller.resultText(), QStringLiteral("final text"));
    QCOMPARE(changedSpy.count(), 1);
}

void SpeechRecognitionControllerTest::finishRejectsEmptyResult()
{
    SpeechRecognitionController controller;

    controller.start(13);
    controller.setPartialText(QStringLiteral("heard something"));
    controller.finish(QStringLiteral("   "));

    QCOMPARE(controller.state(), SpeechRecognitionController::State::Failed);
    QCOMPARE(controller.error(), QStringLiteral("empty result"));
    QCOMPARE(controller.messageId(), qint64(13));
    QVERIFY(controller.resultText().isEmpty());
    QVERIFY(controller.partialText().isEmpty());
    QVERIFY(!controller.active());

    // The same rule applies to the final step of a provisional result.
    controller.start(14);
    controller.finish(QStringLiteral("provisional"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Processing);
    controller.finish(QStringLiteral("\n\t "));

    QCOMPARE(controller.state(), SpeechRecognitionController::State::Failed);
    QCOMPARE(controller.error(), QStringLiteral("empty result"));
    QVERIFY(controller.resultText().isEmpty());
}

void SpeechRecognitionControllerTest::finishIgnoredOutsideListeningAndProcessing()
{
    SpeechRecognitionController controller;
    QSignalSpy changedSpy(&controller, &SpeechRecognitionController::changed);

    controller.finish(QStringLiteral("idle"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Idle);
    QCOMPARE(changedSpy.count(), 0);

    controller.start(15);
    controller.finish(QStringLiteral("done"));
    controller.finish(QStringLiteral("done final"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Done);
    changedSpy.clear();

    controller.finish(QStringLiteral("again"));
    QCOMPARE(controller.resultText(), QStringLiteral("done final"));
    QCOMPARE(changedSpy.count(), 0);

    controller.start(16);
    controller.fail(QStringLiteral("boom"));
    changedSpy.clear();

    controller.finish(QStringLiteral("after failure"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Failed);
    QCOMPARE(controller.error(), QStringLiteral("boom"));
    QCOMPARE(changedSpy.count(), 0);
}

void SpeechRecognitionControllerTest::failOnlyWhileActive()
{
    SpeechRecognitionController controller;
    QSignalSpy changedSpy(&controller, &SpeechRecognitionController::changed);

    controller.fail(QStringLiteral("idle"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Idle);
    QVERIFY(controller.error().isEmpty());
    QCOMPARE(changedSpy.count(), 0);

    controller.start(17);
    controller.finish(QStringLiteral("result"));
    controller.finish(QStringLiteral("final"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Done);
    changedSpy.clear();

    controller.fail(QStringLiteral("too late"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Done);
    QVERIFY(controller.error().isEmpty());
    QCOMPARE(changedSpy.count(), 0);

    controller.start(18);
    controller.setPartialText(QStringLiteral("partial"));
    controller.fail(QStringLiteral("network"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Failed);
    QCOMPARE(controller.error(), QStringLiteral("network"));
    QCOMPARE(controller.messageId(), qint64(18));
    QVERIFY(!controller.active());
    QCOMPARE(changedSpy.count(), 3);

    controller.fail(QStringLiteral("again"));
    QCOMPARE(controller.error(), QStringLiteral("network"));
    QCOMPARE(changedSpy.count(), 3);
}

void SpeechRecognitionControllerTest::failClearsTextAndKeepsMessageId()
{
    SpeechRecognitionController controller;

    // A provisional result is not a result once the attempt has failed.
    controller.start(19);
    controller.finish(QStringLiteral("provisional"));
    QCOMPARE(controller.resultText(), QStringLiteral("provisional"));
    controller.fail(QStringLiteral("engine stopped"));

    QCOMPARE(controller.state(), SpeechRecognitionController::State::Failed);
    QCOMPARE(controller.error(), QStringLiteral("engine stopped"));
    QVERIFY(controller.resultText().isEmpty());
    QCOMPARE(controller.messageId(), qint64(19));

    // An empty error is still a failure; the state carries the outcome.
    controller.start(20);
    controller.fail(QString());
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Failed);
    QVERIFY(controller.error().isEmpty());
}

void SpeechRecognitionControllerTest::cancelResetsEverything()
{
    SpeechRecognitionController controller;

    controller.start(21);
    controller.setPartialText(QStringLiteral("partial"));
    QSignalSpy changedSpy(&controller, &SpeechRecognitionController::changed);

    controller.cancel();

    QCOMPARE(controller.state(), SpeechRecognitionController::State::Idle);
    QCOMPARE(controller.messageId(), qint64(0));
    QVERIFY(controller.partialText().isEmpty());
    QVERIFY(controller.resultText().isEmpty());
    QVERIFY(controller.error().isEmpty());
    QVERIFY(!controller.active());
    QCOMPARE(changedSpy.count(), 1);

    controller.cancel();
    QCOMPARE(changedSpy.count(), 1);

    // From Processing.
    controller.start(22);
    controller.finish(QStringLiteral("provisional"));
    controller.cancel();
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Idle);
    QVERIFY(controller.resultText().isEmpty());

    // From Done.
    controller.start(23);
    controller.finish(QStringLiteral("final"));
    controller.finish(QStringLiteral("done"));
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Done);
    controller.cancel();
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Idle);
    QCOMPARE(controller.messageId(), qint64(0));

    // From Failed.
    controller.start(24);
    controller.fail(QStringLiteral("boom"));
    controller.cancel();
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Idle);
    QVERIFY(controller.error().isEmpty());
}

void SpeechRecognitionControllerTest::stateNamesAreLowercase()
{
    SpeechRecognitionController controller;
    QCOMPARE(controller.stateName(), QStringLiteral("idle"));

    QString requestingName;
    connect(&controller, &SpeechRecognitionController::recognitionRequested, &controller,
        [&controller, &requestingName](qint64) { requestingName = controller.stateName(); });

    controller.start(25);
    QCOMPARE(requestingName, QStringLiteral("requesting"));
    QCOMPARE(controller.stateName(), QStringLiteral("listening"));

    controller.finish(QStringLiteral("provisional"));
    QCOMPARE(controller.stateName(), QStringLiteral("processing"));

    controller.finish(QStringLiteral("final"));
    QCOMPARE(controller.stateName(), QStringLiteral("done"));

    controller.start(26);
    controller.fail(QStringLiteral("boom"));
    QCOMPARE(controller.stateName(), QStringLiteral("failed"));
}

void SpeechRecognitionControllerTest::changedOnlyOnRealChanges()
{
    SpeechRecognitionController controller;
    QSignalSpy changedSpy(&controller, &SpeechRecognitionController::changed);
    QSignalSpy requestSpy(&controller, &SpeechRecognitionController::recognitionRequested);

    controller.start(30); // 1
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 1);

    controller.setPartialText(QStringLiteral("a")); // 2
    controller.setPartialText(QStringLiteral("a")); // no-op
    QCOMPARE(changedSpy.count(), 2);

    controller.finish(QStringLiteral("a")); // 3, Processing
    controller.finish(QStringLiteral("a")); // 4, Done
    controller.finish(QStringLiteral("a")); // no-op
    controller.fail(QStringLiteral("x"));   // no-op
    QCOMPARE(changedSpy.count(), 4);

    controller.cancel(); // 5
    controller.cancel(); // no-op
    QCOMPARE(changedSpy.count(), 5);

    controller.start(31);                           // 6
    controller.setPartialText(QStringLiteral("b")); // 7
    controller.fail(QStringLiteral("boom"));        // 8
    controller.fail(QStringLiteral("boom"));        // no-op
    QCOMPARE(changedSpy.count(), 8);

    controller.cancel(); // 9
    controller.start(32); // 10
    changedSpy.clear();
    requestSpy.clear();

    // A repeated start for the same message still asks the owner to begin
    // again, but with no text to clear it changes nothing observable.
    controller.start(32);
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 0);

    controller.setPartialText(QStringLiteral("c"));
    changedSpy.clear();
    controller.start(32);
    QCOMPARE(controller.state(), SpeechRecognitionController::State::Listening);
    QVERIFY(controller.partialText().isEmpty());
    QCOMPARE(changedSpy.count(), 1);
}

QTEST_GUILESS_MAIN(SpeechRecognitionControllerTest)
#include "tst_speech_recognition_controller.moc"
