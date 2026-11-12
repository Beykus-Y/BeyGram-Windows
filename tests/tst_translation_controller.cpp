#include "telegram/TranslationController.h"

#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

TranslatedMessage translation(qint64 messageId, const QString& language,
                              const QString& text)
{
    TranslatedMessage message;
    message.messageId = messageId;
    message.language = language;
    message.text = text;
    return message;
}

} // namespace

class TranslationControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setTargetLanguageTrimsAndLowercases();
    void setTargetLanguageAcceptsEmpty();
    void setTargetLanguageEmitsOnlyOnChange();
    void requestIgnoresInvalidIds();
    void requestMarksLoading();
    void requestIgnoresCachedMessage();
    void requestIsIdempotent();
    void requestIsIgnoredWhileDisabled();
    void setTranslationStoresText();
    void setTranslationWorksWithoutRequest();
    void setTranslationRejectsInvalidId();
    void setTranslationRejectsEmptyText();
    void setTranslationRejectsWrongLanguage();
    void setTranslationAcceptsLanguageVariants();
    void setTranslationKeepsLoadingWhenRejected();
    void setTranslationIsNoOpWhenUnchanged();
    void translationForIsEmptyWhenMissing();
    void isLoadingFollowsRequestAndResult();
    void clearTranslationDropsTextAndLoading();
    void clearTranslationIgnoresInvalidInput();
    void clearAllDropsTextsAndLoading();
    void clearAllKeepsLanguage();
    void clearAllIsNoOpWhenEmpty();
    void cachedCountCountsTranslatedMessages();
    void changingLanguageKeepsCachedTranslations();
    void emitsChangedOnlyOnRealChanges();
};

void TranslationControllerTest::startsEmpty()
{
    TranslationController controller;

    QCOMPARE(controller.targetLanguage(), QString());
    QCOMPARE(controller.cachedCount(), 0);
    QVERIFY(controller.translationFor(1).isEmpty());
    QVERIFY(!controller.isLoading(1));
    QVERIFY(controller.translationFor(9876543210LL).isEmpty());
}

void TranslationControllerTest::setTargetLanguageTrimsAndLowercases()
{
    TranslationController controller;

    controller.setTargetLanguage(QStringLiteral("  EN  "));
    QCOMPARE(controller.targetLanguage(), QStringLiteral("en"));

    controller.setTargetLanguage(QStringLiteral("Pt-Br"));
    QCOMPARE(controller.targetLanguage(), QStringLiteral("pt-br"));

    controller.setTargetLanguage(QStringLiteral("de"));
    QCOMPARE(controller.targetLanguage(), QStringLiteral("de"));
}

void TranslationControllerTest::setTargetLanguageAcceptsEmpty()
{
    TranslationController controller;

    controller.setTargetLanguage(QStringLiteral("fr"));
    controller.setTargetLanguage(QString());
    QCOMPARE(controller.targetLanguage(), QString());

    // Whitespace normalises to the same disabled state and is not a new value.
    QSignalSpy spy(&controller, &TranslationController::changed);
    controller.setTargetLanguage(QStringLiteral("   "));
    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.targetLanguage(), QString());

    controller.setTargetLanguage(QStringLiteral("fr"));
    QCOMPARE(controller.targetLanguage(), QStringLiteral("fr"));
}

void TranslationControllerTest::setTargetLanguageEmitsOnlyOnChange()
{
    TranslationController controller;
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.setTargetLanguage(QStringLiteral("en"));      // 1
    controller.setTargetLanguage(QStringLiteral("en"));      // no-op
    controller.setTargetLanguage(QStringLiteral(" EN "));    // normalises -> no-op
    controller.setTargetLanguage(QStringLiteral("de"));      // 2
    controller.setTargetLanguage(QString());                 // 3
    controller.setTargetLanguage(QString());                 // no-op

    QCOMPARE(spy.count(), 3);
}

void TranslationControllerTest::requestIgnoresInvalidIds()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.request(0);
    controller.request(-1);
    controller.request(-9876543210LL);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.isLoading(0));
    QVERIFY(!controller.isLoading(-1));
    QCOMPARE(controller.cachedCount(), 0);
}

void TranslationControllerTest::requestMarksLoading()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.request(42);

    QVERIFY(controller.isLoading(42));
    QVERIFY(!controller.isLoading(43));
    QVERIFY(controller.translationFor(42).isEmpty());
    QCOMPARE(controller.cachedCount(), 0);
    QCOMPARE(spy.count(), 1);

    controller.request(9876543210LL);
    QVERIFY(controller.isLoading(9876543210LL));
    QCOMPARE(spy.count(), 2);
}

void TranslationControllerTest::requestIgnoresCachedMessage()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    controller.setTranslation(
        translation(7, QStringLiteral("en"), QStringLiteral("seven")));

    QSignalSpy spy(&controller, &TranslationController::changed);
    controller.request(7);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.isLoading(7));
    QCOMPARE(controller.translationFor(7), QStringLiteral("seven"));
}

void TranslationControllerTest::requestIsIdempotent()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    controller.request(5);

    QSignalSpy spy(&controller, &TranslationController::changed);
    controller.request(5);
    controller.request(5);

    QCOMPARE(spy.count(), 0);
    QVERIFY(controller.isLoading(5));
}

void TranslationControllerTest::requestIsIgnoredWhileDisabled()
{
    TranslationController controller;
    QSignalSpy spy(&controller, &TranslationController::changed);

    // No target language yet: nothing to request, nothing to leave pending.
    controller.request(1);
    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.isLoading(1));

    controller.setTargetLanguage(QStringLiteral("en"));
    controller.request(1);
    QVERIFY(controller.isLoading(1));

    // Disabling does not cancel an in-flight request, but blocks new ones.
    controller.setTargetLanguage(QString());
    QCOMPARE(spy.count(), 3);
    controller.request(2);
    QCOMPARE(spy.count(), 3);
    QVERIFY(!controller.isLoading(2));
    QVERIFY(controller.isLoading(1));
}

void TranslationControllerTest::setTranslationStoresText()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    controller.request(11);
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.setTranslation(
        translation(11, QStringLiteral("en"), QStringLiteral("hello")));

    QCOMPARE(controller.translationFor(11), QStringLiteral("hello"));
    QVERIFY(!controller.isLoading(11));
    QCOMPARE(controller.cachedCount(), 1);
    QCOMPARE(spy.count(), 1);
}

void TranslationControllerTest::setTranslationWorksWithoutRequest()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));

    controller.setTranslation(
        translation(3, QStringLiteral("en"), QStringLiteral("three")));

    QCOMPARE(controller.translationFor(3), QStringLiteral("three"));
    QVERIFY(!controller.isLoading(3));
    QCOMPARE(controller.cachedCount(), 1);
}

void TranslationControllerTest::setTranslationRejectsInvalidId()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.setTranslation(
        translation(0, QStringLiteral("en"), QStringLiteral("zero")));
    controller.setTranslation(
        translation(-4, QStringLiteral("en"), QStringLiteral("negative")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.cachedCount(), 0);
    QVERIFY(controller.translationFor(0).isEmpty());
    QVERIFY(controller.translationFor(-4).isEmpty());
}

void TranslationControllerTest::setTranslationRejectsEmptyText()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    controller.request(8);
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.setTranslation(translation(8, QStringLiteral("en"), QString()));
    controller.setTranslation(
        translation(8, QStringLiteral("en"), QStringLiteral("   \n\t ")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.cachedCount(), 0);
    QVERIFY(controller.translationFor(8).isEmpty());
    QVERIFY(controller.isLoading(8));
}

void TranslationControllerTest::setTranslationRejectsWrongLanguage()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.setTranslation(
        translation(1, QStringLiteral("de"), QStringLiteral("hallo")));
    controller.setTranslation(
        translation(1, QString(), QStringLiteral("hola")));
    controller.setTranslation(
        translation(1, QStringLiteral("   "), QStringLiteral("bonjour")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.cachedCount(), 0);
    QVERIFY(controller.translationFor(1).isEmpty());

    // With translation disabled no language can match.
    controller.setTargetLanguage(QString());
    controller.setTranslation(
        translation(1, QStringLiteral("en"), QStringLiteral("hello")));
    QCOMPARE(controller.cachedCount(), 0);
    QVERIFY(controller.translationFor(1).isEmpty());
}

void TranslationControllerTest::setTranslationAcceptsLanguageVariants()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("PT-BR"));

    controller.setTranslation(
        translation(1, QStringLiteral(" pt-br "), QStringLiteral("olá")));

    QCOMPARE(controller.translationFor(1), QStringLiteral("olá"));
    QCOMPARE(controller.cachedCount(), 1);
}

void TranslationControllerTest::setTranslationKeepsLoadingWhenRejected()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    controller.request(21);
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.setTranslation(
        translation(21, QStringLiteral("fr"), QStringLiteral("bonjour")));
    QCOMPARE(spy.count(), 0);
    QVERIFY(controller.isLoading(21));

    controller.setTranslation(
        translation(21, QStringLiteral("en"), QStringLiteral("hello")));
    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.isLoading(21));
    QCOMPARE(controller.translationFor(21), QStringLiteral("hello"));
}

void TranslationControllerTest::setTranslationIsNoOpWhenUnchanged()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    controller.setTranslation(
        translation(4, QStringLiteral("en"), QStringLiteral("four")));

    QSignalSpy spy(&controller, &TranslationController::changed);
    controller.setTranslation(
        translation(4, QStringLiteral("en"), QStringLiteral("four")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.translationFor(4), QStringLiteral("four"));

    // A different text for the same message is a real change.
    controller.setTranslation(
        translation(4, QStringLiteral("en"), QStringLiteral("FOUR")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.translationFor(4), QStringLiteral("FOUR"));
    QCOMPARE(controller.cachedCount(), 1);
}

void TranslationControllerTest::translationForIsEmptyWhenMissing()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    controller.setTranslation(
        translation(1, QStringLiteral("en"), QStringLiteral("one")));

    QCOMPARE(controller.translationFor(1), QStringLiteral("one"));
    QVERIFY(controller.translationFor(2).isEmpty());
    QVERIFY(controller.translationFor(0).isEmpty());
    QVERIFY(controller.translationFor(-1).isEmpty());
    QVERIFY(controller.translationFor(9876543210LL).isEmpty());
}

void TranslationControllerTest::isLoadingFollowsRequestAndResult()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));

    QVERIFY(!controller.isLoading(9));
    controller.request(9);
    QVERIFY(controller.isLoading(9));

    controller.setTranslation(
        translation(9, QStringLiteral("en"), QStringLiteral("nine")));
    QVERIFY(!controller.isLoading(9));

    controller.request(9);
    QVERIFY(!controller.isLoading(9));
}

void TranslationControllerTest::clearTranslationDropsTextAndLoading()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    controller.setTranslation(
        translation(1, QStringLiteral("en"), QStringLiteral("one")));
    controller.request(2);
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.clearTranslation(1);
    QVERIFY(controller.translationFor(1).isEmpty());
    QCOMPARE(controller.cachedCount(), 0);
    QCOMPARE(spy.count(), 1);

    controller.clearTranslation(2);
    QVERIFY(!controller.isLoading(2));
    QCOMPARE(spy.count(), 2);

    // A message that is only loading loses that flag through the same call.
    controller.request(4);
    controller.clearTranslation(4);
    QVERIFY(!controller.isLoading(4));
}

void TranslationControllerTest::clearTranslationIgnoresInvalidInput()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    controller.setTranslation(
        translation(1, QStringLiteral("en"), QStringLiteral("one")));
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.clearTranslation(0);
    controller.clearTranslation(-2);
    controller.clearTranslation(99);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.cachedCount(), 1);
    QCOMPARE(controller.translationFor(1), QStringLiteral("one"));
}

void TranslationControllerTest::clearAllDropsTextsAndLoading()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    controller.setTranslation(
        translation(1, QStringLiteral("en"), QStringLiteral("one")));
    controller.setTranslation(
        translation(2, QStringLiteral("en"), QStringLiteral("two")));
    controller.request(3);
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.clearAll();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.cachedCount(), 0);
    QVERIFY(controller.translationFor(1).isEmpty());
    QVERIFY(controller.translationFor(2).isEmpty());
    QVERIFY(!controller.isLoading(3));
}

void TranslationControllerTest::clearAllKeepsLanguage()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    controller.request(1);

    controller.clearAll();

    QCOMPARE(controller.targetLanguage(), QStringLiteral("en"));
    QVERIFY(!controller.isLoading(1));
}

void TranslationControllerTest::clearAllIsNoOpWhenEmpty()
{
    TranslationController controller;
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.clearAll();
    controller.clearAll();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.cachedCount(), 0);
}

void TranslationControllerTest::cachedCountCountsTranslatedMessages()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));

    controller.request(1);
    controller.request(2);
    controller.request(3);
    QCOMPARE(controller.cachedCount(), 0);

    controller.setTranslation(
        translation(1, QStringLiteral("en"), QStringLiteral("one")));
    QCOMPARE(controller.cachedCount(), 1);

    controller.setTranslation(
        translation(2, QStringLiteral("en"), QStringLiteral("two")));
    QCOMPARE(controller.cachedCount(), 2);

    // Updating the same message must not add a second entry.
    controller.setTranslation(
        translation(2, QStringLiteral("en"), QStringLiteral("TWO")));
    QCOMPARE(controller.cachedCount(), 2);

    // A rejected translation does not count.
    controller.setTranslation(
        translation(3, QStringLiteral("de"), QStringLiteral("drei")));
    QCOMPARE(controller.cachedCount(), 2);

    controller.clearTranslation(1);
    QCOMPARE(controller.cachedCount(), 1);
}

void TranslationControllerTest::changingLanguageKeepsCachedTranslations()
{
    TranslationController controller;
    controller.setTargetLanguage(QStringLiteral("en"));
    controller.setTranslation(
        translation(1, QStringLiteral("en"), QStringLiteral("hello")));

    controller.setTargetLanguage(QStringLiteral("de"));
    QCOMPARE(controller.cachedCount(), 1);
    QCOMPARE(controller.translationFor(1), QStringLiteral("hello"));

    // A late reply for the previous language is now rejected.
    controller.setTranslation(
        translation(2, QStringLiteral("en"), QStringLiteral("world")));
    QCOMPARE(controller.cachedCount(), 1);
    QVERIFY(controller.translationFor(2).isEmpty());

    controller.clearAll();
    QCOMPARE(controller.cachedCount(), 0);
    QVERIFY(controller.translationFor(1).isEmpty());
}

void TranslationControllerTest::emitsChangedOnlyOnRealChanges()
{
    TranslationController controller;
    QSignalSpy spy(&controller, &TranslationController::changed);

    controller.setTargetLanguage(QStringLiteral("EN"));       // 1
    controller.setTargetLanguage(QStringLiteral(" en "));     // no-op
    controller.request(1);                                    // 2
    controller.request(1);                                    // no-op
    controller.request(0);                                    // no-op
    controller.request(-5);                                   // no-op
    controller.setTranslation(
        translation(1, QStringLiteral("en"), QStringLiteral("hello"))); // 3
    controller.setTranslation(
        translation(1, QStringLiteral("en"), QStringLiteral("hello"))); // no-op
    controller.request(1);                                    // no-op, cached
    controller.request(2);                                    // 4
    controller.setTranslation(
        translation(2, QStringLiteral("de"), QStringLiteral("hallo"))); // no-op
    controller.clearTranslation(2);                           // 5
    controller.clearTranslation(2);                           // no-op
    controller.setTranslation(
        translation(3, QStringLiteral("en"), QStringLiteral("three"))); // 6
    controller.clearAll();                                    // 7
    controller.clearAll();                                    // no-op
    controller.setTargetLanguage(QString());                  // 8
    controller.setTargetLanguage(QString());                  // no-op

    QCOMPARE(spy.count(), 8);
    QCOMPARE(controller.targetLanguage(), QString());
    QCOMPARE(controller.cachedCount(), 0);
    QVERIFY(!controller.isLoading(2));
}

QTEST_GUILESS_MAIN(TranslationControllerTest)
#include "tst_translation_controller.moc"
