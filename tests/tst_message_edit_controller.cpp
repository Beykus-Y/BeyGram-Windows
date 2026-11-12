#include "telegram/MessageEditController.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class MessageEditControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();
    void beginActivatesEdit();
    void beginRequiresValidIds();
    void beginRequiresNonBlankText();
    void beginReplacesExistingEdit();
    void cancelClearsEdit();
    void cancelRequiresActiveEdit();
    void setTextRequiresActiveEdit();
    void setTextKeepsTextVerbatim();
    void dirtyComparesExactText();
    void canSaveRequiresDirtyDraft();
    void canSaveRejectsBlankDraft();
    void canSaveEnforcesLengthLimit();
    void markSavedAcceptsDraft();
    void markSavedRequiresDirtyDraft();
    void emitsChangedOnlyOnRealChanges();
};

void MessageEditControllerTest::startsInactive()
{
    const MessageEditController controller;

    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.messageId(), qint64(0));
    QVERIFY(controller.text().isEmpty());
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());
}

void MessageEditControllerTest::beginActivatesEdit()
{
    MessageEditController controller;

    controller.begin(42, 700, QStringLiteral("hello"));

    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(42));
    QCOMPARE(controller.messageId(), qint64(700));
    QCOMPARE(controller.text(), QStringLiteral("hello"));
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());
}

void MessageEditControllerTest::beginRequiresValidIds()
{
    MessageEditController controller;

    controller.begin(0, 10, QStringLiteral("hello"));
    controller.begin(-1, 10, QStringLiteral("hello"));
    controller.begin(1, 0, QStringLiteral("hello"));
    controller.begin(1, -10, QStringLiteral("hello"));
    controller.begin(0, 0, QStringLiteral("hello"));

    QVERIFY(!controller.active());
    QVERIFY(controller.text().isEmpty());

    // An invalid begin must not disturb an edit that is already in flight.
    controller.begin(5, 50, QStringLiteral("draft"));
    controller.setText(QStringLiteral("changed"));
    controller.begin(0, 60, QStringLiteral("other"));
    controller.begin(6, -1, QStringLiteral("other"));

    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(5));
    QCOMPARE(controller.messageId(), qint64(50));
    QCOMPARE(controller.text(), QStringLiteral("changed"));
}

void MessageEditControllerTest::beginRequiresNonBlankText()
{
    MessageEditController controller;

    controller.begin(1, 10, QString());
    controller.begin(1, 10, QStringLiteral("   "));
    controller.begin(1, 10, QStringLiteral("\n\t "));

    QVERIFY(!controller.active());
    QVERIFY(controller.text().isEmpty());

    // Text with surrounding whitespace is accepted verbatim.
    controller.begin(1, 10, QStringLiteral("  hi  "));
    QVERIFY(controller.active());
    QCOMPARE(controller.text(), QStringLiteral("  hi  "));
    QVERIFY(!controller.dirty());
}

void MessageEditControllerTest::beginReplacesExistingEdit()
{
    MessageEditController controller;

    controller.begin(1, 10, QStringLiteral("first"));
    controller.setText(QStringLiteral("draft"));
    controller.begin(2, 20, QStringLiteral("second"));

    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(2));
    QCOMPARE(controller.messageId(), qint64(20));
    QCOMPARE(controller.text(), QStringLiteral("second"));
    QVERIFY(!controller.dirty());
}

void MessageEditControllerTest::cancelClearsEdit()
{
    MessageEditController controller;

    controller.begin(3, 30, QStringLiteral("hello"));
    controller.setText(QStringLiteral("bye"));
    controller.cancel();

    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.messageId(), qint64(0));
    QVERIFY(controller.text().isEmpty());
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());

    // A new edit can start after cancelling.
    controller.begin(4, 40, QStringLiteral("again"));
    QVERIFY(controller.active());
    QCOMPARE(controller.text(), QStringLiteral("again"));
}

void MessageEditControllerTest::cancelRequiresActiveEdit()
{
    MessageEditController controller;
    QSignalSpy spy(&controller, &MessageEditController::changed);

    controller.cancel();
    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.active());

    controller.begin(3, 30, QStringLiteral("hello"));
    QCOMPARE(spy.count(), 1);
    controller.cancel();
    QCOMPARE(spy.count(), 2);

    // Cancelling twice is a no-op the second time.
    controller.cancel();
    QCOMPARE(spy.count(), 2);
}

void MessageEditControllerTest::setTextRequiresActiveEdit()
{
    MessageEditController controller;

    controller.setText(QStringLiteral("orphan"));
    QVERIFY(controller.text().isEmpty());
    QVERIFY(!controller.dirty());

    controller.begin(3, 30, QStringLiteral("hello"));
    controller.cancel();
    controller.setText(QStringLiteral("orphan"));
    QVERIFY(controller.text().isEmpty());
    QVERIFY(!controller.active());
}

void MessageEditControllerTest::setTextKeepsTextVerbatim()
{
    MessageEditController controller;

    controller.begin(3, 30, QStringLiteral("hello"));

    // No trimming happens on the way in.
    controller.setText(QStringLiteral("  padded  "));
    QCOMPARE(controller.text(), QStringLiteral("  padded  "));
    QVERIFY(controller.dirty());

    controller.setText(QStringLiteral("   "));
    QCOMPARE(controller.text(), QStringLiteral("   "));
    QVERIFY(controller.dirty());
}

void MessageEditControllerTest::dirtyComparesExactText()
{
    MessageEditController controller;

    controller.begin(3, 30, QStringLiteral("hello"));
    QVERIFY(!controller.dirty());

    // Whitespace differences count as edits.
    controller.setText(QStringLiteral("hello "));
    QVERIFY(controller.dirty());

    controller.setText(QStringLiteral("hello"));
    QVERIFY(!controller.dirty());

    controller.setText(QStringLiteral("Hello"));
    QVERIFY(controller.dirty());

    controller.setText(QString());
    QVERIFY(controller.dirty());
}

void MessageEditControllerTest::canSaveRequiresDirtyDraft()
{
    MessageEditController controller;

    QVERIFY(!controller.canSave());

    controller.begin(3, 30, QStringLiteral("hello"));
    QVERIFY(!controller.canSave());

    controller.setText(QStringLiteral("hello!"));
    QVERIFY(controller.canSave());
    QVERIFY(controller.dirty());

    controller.setText(QStringLiteral("hello"));
    QVERIFY(!controller.canSave());

    controller.cancel();
    QVERIFY(!controller.canSave());
}

void MessageEditControllerTest::canSaveRejectsBlankDraft()
{
    MessageEditController controller;

    controller.begin(3, 30, QStringLiteral("hello"));

    controller.setText(QString());
    QVERIFY(controller.dirty());
    QVERIFY(!controller.canSave());

    controller.setText(QStringLiteral("  \n\t "));
    QVERIFY(controller.dirty());
    QVERIFY(!controller.canSave());

    // Surrounding whitespace alone does not make a draft blank.
    controller.setText(QStringLiteral("  x  "));
    QVERIFY(controller.canSave());
}

void MessageEditControllerTest::canSaveEnforcesLengthLimit()
{
    MessageEditController controller;

    controller.begin(3, 30, QStringLiteral("hello"));

    const QString atLimit(MessageEditController::kMaxTextLength, QLatin1Char('a'));
    controller.setText(atLimit);
    QCOMPARE(controller.text().length(), MessageEditController::kMaxTextLength);
    QVERIFY(controller.canSave());

    const QString overLimit(MessageEditController::kMaxTextLength + 1, QLatin1Char('a'));
    controller.setText(overLimit);
    QVERIFY(controller.dirty());
    QVERIFY(!controller.canSave());

    // The limit applies to the exact length, not the trimmed length.
    controller.setText(QString(MessageEditController::kMaxTextLength, QLatin1Char('a'))
                       + QStringLiteral("  "));
    QVERIFY(!controller.canSave());
}

void MessageEditControllerTest::markSavedAcceptsDraft()
{
    MessageEditController controller;

    controller.begin(3, 30, QStringLiteral("hello"));
    controller.setText(QStringLiteral("  hello!  "));
    QVERIFY(controller.dirty());

    controller.markSaved();

    QVERIFY(controller.active());
    QCOMPARE(controller.text(), QStringLiteral("  hello!  "));
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());

    // Further edits are again measured against the accepted text.
    controller.setText(QStringLiteral("  hello!  "));
    QVERIFY(!controller.dirty());
    controller.setText(QStringLiteral("  hello?  "));
    QVERIFY(controller.dirty());
    QVERIFY(controller.canSave());
}

void MessageEditControllerTest::markSavedRequiresDirtyDraft()
{
    MessageEditController controller;
    QSignalSpy spy(&controller, &MessageEditController::changed);

    controller.markSaved();
    QCOMPARE(spy.count(), 0);

    controller.begin(3, 30, QStringLiteral("hello"));
    QCOMPARE(spy.count(), 1);

    // Nothing changed, so accepting the draft is a no-op.
    controller.markSaved();
    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.dirty());

    controller.setText(QStringLiteral("hello!"));
    QCOMPARE(spy.count(), 2);
    controller.markSaved();
    QCOMPARE(spy.count(), 3);
    QVERIFY(!controller.dirty());
}

void MessageEditControllerTest::emitsChangedOnlyOnRealChanges()
{
    MessageEditController controller;
    QSignalSpy spy(&controller, &MessageEditController::changed);

    controller.begin(3, 30, QStringLiteral("hello"));
    QCOMPARE(spy.count(), 1);

    // Repeating the same edit changes nothing.
    controller.begin(3, 30, QStringLiteral("hello"));
    QCOMPARE(spy.count(), 1);

    controller.setText(QStringLiteral("hello!"));
    QCOMPARE(spy.count(), 2);

    // Setting the same text again is a no-op.
    controller.setText(QStringLiteral("hello!"));
    QCOMPARE(spy.count(), 2);

    // Invalid input stays silent.
    controller.begin(0, 30, QStringLiteral("ignored"));
    controller.begin(3, -1, QStringLiteral("ignored"));
    controller.begin(3, 30, QStringLiteral("   "));
    QCOMPARE(spy.count(), 2);

    controller.markSaved();
    QCOMPARE(spy.count(), 3);

    // Saving again without new edits is silent.
    controller.markSaved();
    QCOMPARE(spy.count(), 3);

    controller.begin(4, 40, QStringLiteral("other"));
    QCOMPARE(spy.count(), 4);

    controller.cancel();
    QCOMPARE(spy.count(), 5);

    // Cancelling or editing without an active session is silent.
    controller.cancel();
    controller.setText(QStringLiteral("late"));
    QCOMPARE(spy.count(), 5);

    controller.begin(5, 50, QStringLiteral("again"));
    QCOMPARE(spy.count(), 6);
}

QTEST_GUILESS_MAIN(MessageEditControllerTest)
#include "tst_message_edit_controller.moc"
