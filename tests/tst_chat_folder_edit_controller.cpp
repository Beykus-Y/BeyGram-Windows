#include "telegram/ChatFolderEditController.h"

#include "telegram/FolderEdit.h"

#include <QSignalSpy>
#include <QString>
#include <QtTest>

using beygram::telegram::ChatFolderEditController;
using beygram::telegram::createChatFolderRequest;

namespace {

/// The name createChatFolder would actually carry, read the same way the
/// controller normalises its draft.
QString payloadTitle(const QString& title)
{
    return createChatFolderRequest(title)
        .value(QStringLiteral("folder"))
        .toObject()
        .value(QStringLiteral("name"))
        .toObject()
        .value(QStringLiteral("text"))
        .toObject()
        .value(QStringLiteral("text"))
        .toString();
}

/// One emoji outside the BMP: two UTF-16 units, one code point.
QString smiley()
{
    return QStringLiteral("\U0001F600");
}

} // namespace

class ChatFolderEditControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();
    void beginActivatesNewFolder();
    void beginActivatesExistingFolder();
    void beginTrimsTitle();
    void beginFoldsWhitespace();
    void beginCapsTitleAtTwelveCodePoints();
    void beginKeepsSurrogatePairsWhole();
    void beginWithBlankTitleCannotSave();
    void beginIsSilentWhenNothingChanges();
    void beginSignalsTitleChange();
    void beginDiscardsDraftOfPreviousFolder();
    void setTitleMarksDirty();
    void setTitleIsIgnoredWhenInactive();
    void setTitleTrimsAndFolds();
    void setTitleCapsAtTwelveCodePoints();
    void setTitleIsSilentOnEquivalentTitle();
    void setTitleBackToOriginalIsClean();
    void setTitleToBlankDisablesSave();
    void canSaveRequiresActive();
    void canSaveRequiresDirty();
    void canSaveAcceptsTwelveCodePoints();
    void cancelDeactivates();
    void cancelIsSilentWhenInactive();
    void markSavedClearsDirty();
    void markSavedKeepsTitleForFurtherEdits();
    void markSavedIsSilentWhenClean();
    void markSavedIsIgnoredWhenInactive();
    void changedOnlyOnRealChanges();
    void titleMatchesFolderEditPayload();
};

void ChatFolderEditControllerTest::startsInactive()
{
    ChatFolderEditController controller;

    QVERIFY(!controller.active());
    QCOMPARE(controller.folderId(), qint64(-1));
    QCOMPARE(controller.title(), QString());
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());
}

void ChatFolderEditControllerTest::beginActivatesNewFolder()
{
    // 0 is the id a folder has before the server created it.
    ChatFolderEditController controller;
    QSignalSpy spy(&controller, &ChatFolderEditController::changed);

    controller.begin(0, QStringLiteral("Work"));

    QVERIFY(controller.active());
    QCOMPARE(controller.folderId(), qint64(0));
    QCOMPARE(controller.title(), QStringLiteral("Work"));
    // Beginning an edit is not editing it: nothing to save yet.
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());
    QCOMPARE(spy.count(), 1);
}

void ChatFolderEditControllerTest::beginActivatesExistingFolder()
{
    ChatFolderEditController controller;
    controller.begin(42, QStringLiteral("Work"));

    QVERIFY(controller.active());
    QCOMPARE(controller.folderId(), qint64(42));
    QVERIFY(!controller.dirty());
}

void ChatFolderEditControllerTest::beginTrimsTitle()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("  Work \t "));

    QCOMPARE(controller.title(), QStringLiteral("Work"));
    QVERIFY(!controller.dirty());
}

void ChatFolderEditControllerTest::beginFoldsWhitespace()
{
    // A folder name may not contain line feeds, and the builder folds every
    // whitespace run into one space, so the draft starts out already folded.
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("  Work\n\t chats "));

    QCOMPARE(controller.title(), QStringLiteral("Work chats"));
    QVERIFY(!controller.dirty());
}

void ChatFolderEditControllerTest::beginCapsTitleAtTwelveCodePoints()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("abcdefghijklmnop"));

    QCOMPARE(controller.title(), QStringLiteral("abcdefghijkl"));
    QCOMPARE(controller.title().size(), 12);
    QVERIFY(!controller.dirty());
}

void ChatFolderEditControllerTest::beginKeepsSurrogatePairsWhole()
{
    // Eleven ASCII characters plus one emoji is twelve characters; the cut must
    // not leave half a surrogate pair behind.
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("aaaaaaaaaaa") + smiley());

    QCOMPARE(controller.title().size(), 13);
    QVERIFY(controller.title().at(11).isHighSurrogate());
    QVERIFY(controller.title().at(12).isLowSurrogate());

    ChatFolderEditController full;
    full.begin(1, QStringLiteral("aaaaaaaaaaaa") + smiley());
    QCOMPARE(full.title(), QStringLiteral("aaaaaaaaaaaa"));
    QCOMPARE(full.title().size(), 12);
}

void ChatFolderEditControllerTest::beginWithBlankTitleCannotSave()
{
    ChatFolderEditController controller;
    controller.begin(0, QStringLiteral("   \n "));

    QVERIFY(controller.active());
    QCOMPARE(controller.title(), QString());
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());
}

void ChatFolderEditControllerTest::beginIsSilentWhenNothingChanges()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("Work"));
    QSignalSpy spy(&controller, &ChatFolderEditController::changed);

    controller.begin(1, QStringLiteral(" Work "));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.dirty());
}

void ChatFolderEditControllerTest::beginSignalsTitleChange()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("Work"));
    QSignalSpy spy(&controller, &ChatFolderEditController::changed);

    controller.begin(1, QStringLiteral("Play"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.title(), QStringLiteral("Play"));
    // Re-opening a folder loads its stored name, so it is clean again.
    QVERIFY(!controller.dirty());
}

void ChatFolderEditControllerTest::beginDiscardsDraftOfPreviousFolder()
{
    ChatFolderEditController controller;
    controller.begin(0, QStringLiteral("Draft"));
    controller.setTitle(QStringLiteral("Typed"));
    QVERIFY(controller.dirty());

    controller.begin(5, QStringLiteral("Saved"));

    QCOMPARE(controller.folderId(), qint64(5));
    QCOMPARE(controller.title(), QStringLiteral("Saved"));
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());
}

void ChatFolderEditControllerTest::setTitleMarksDirty()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("Old"));
    QSignalSpy spy(&controller, &ChatFolderEditController::changed);

    controller.setTitle(QStringLiteral("New"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.title(), QStringLiteral("New"));
    QVERIFY(controller.dirty());
    QVERIFY(controller.canSave());
}

void ChatFolderEditControllerTest::setTitleIsIgnoredWhenInactive()
{
    ChatFolderEditController controller;
    QSignalSpy spy(&controller, &ChatFolderEditController::changed);

    controller.setTitle(QStringLiteral("Work"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.active());
    QCOMPARE(controller.title(), QString());
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());
}

void ChatFolderEditControllerTest::setTitleTrimsAndFolds()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("Old"));
    controller.setTitle(QStringLiteral("  New\n name \t"));

    QCOMPARE(controller.title(), QStringLiteral("New name"));
    QVERIFY(controller.dirty());
}

void ChatFolderEditControllerTest::setTitleCapsAtTwelveCodePoints()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("Old"));
    controller.setTitle(QStringLiteral("abcdefghijklmnop"));

    QCOMPARE(controller.title(), QStringLiteral("abcdefghijkl"));
    QVERIFY(controller.dirty());
    QVERIFY(controller.canSave());
}

void ChatFolderEditControllerTest::setTitleIsSilentOnEquivalentTitle()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("Work"));
    QSignalSpy spy(&controller, &ChatFolderEditController::changed);

    controller.setTitle(QStringLiteral(" Work "));
    controller.setTitle(QStringLiteral("Work"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.dirty());
}

void ChatFolderEditControllerTest::setTitleBackToOriginalIsClean()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("Work"));
    controller.setTitle(QStringLiteral("Play"));
    QVERIFY(controller.canSave());

    controller.setTitle(QStringLiteral(" Work "));

    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());
}

void ChatFolderEditControllerTest::setTitleToBlankDisablesSave()
{
    // An emptied field is a change, but TDLib rejects a nameless folder, so the
    // Save button has to stay off.
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("Work"));

    controller.setTitle(QStringLiteral("   "));

    QCOMPARE(controller.title(), QString());
    QVERIFY(controller.dirty());
    QVERIFY(!controller.canSave());
}

void ChatFolderEditControllerTest::canSaveRequiresActive()
{
    ChatFolderEditController controller;
    QVERIFY(!controller.canSave());

    controller.begin(0, QStringLiteral("Work"));
    controller.setTitle(QStringLiteral("Play"));
    QVERIFY(controller.canSave());

    controller.cancel();
    QVERIFY(!controller.canSave());
}

void ChatFolderEditControllerTest::canSaveRequiresDirty()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("Work"));

    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());
}

void ChatFolderEditControllerTest::canSaveAcceptsTwelveCodePoints()
{
    // Twelve code points are within the limit even though they occupy thirteen
    // UTF-16 units; counting UTF-16 units would wrongly disable Save here.
    ChatFolderEditController controller;
    controller.begin(7, QString());
    controller.setTitle(QStringLiteral("aaaaaaaaaaa") + smiley());

    QCOMPARE(controller.title().size(), 13);
    QVERIFY(controller.dirty());
    QVERIFY(controller.canSave());
}

void ChatFolderEditControllerTest::cancelDeactivates()
{
    ChatFolderEditController controller;
    controller.begin(3, QStringLiteral("Work"));
    controller.setTitle(QStringLiteral("Play"));
    QSignalSpy spy(&controller, &ChatFolderEditController::changed);

    controller.cancel();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.active());
    QCOMPARE(controller.folderId(), qint64(-1));
    QCOMPARE(controller.title(), QString());
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());
}

void ChatFolderEditControllerTest::cancelIsSilentWhenInactive()
{
    ChatFolderEditController controller;
    QSignalSpy spy(&controller, &ChatFolderEditController::changed);

    controller.cancel();
    controller.cancel();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.active());
}

void ChatFolderEditControllerTest::markSavedClearsDirty()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("Old"));
    controller.setTitle(QStringLiteral("New"));
    QSignalSpy spy(&controller, &ChatFolderEditController::changed);

    controller.markSaved();

    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.active());
    QCOMPARE(controller.folderId(), qint64(1));
    QCOMPARE(controller.title(), QStringLiteral("New"));
    QVERIFY(!controller.dirty());
    QVERIFY(!controller.canSave());
}

void ChatFolderEditControllerTest::markSavedKeepsTitleForFurtherEdits()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("Old"));
    controller.setTitle(QStringLiteral("New"));
    controller.markSaved();

    // The next edit is measured against what was just saved, not against the
    // name the folder was opened with.
    controller.setTitle(QStringLiteral("Old"));

    QVERIFY(controller.dirty());
    QVERIFY(controller.canSave());

    controller.setTitle(QStringLiteral("New"));
    QVERIFY(!controller.dirty());
}

void ChatFolderEditControllerTest::markSavedIsSilentWhenClean()
{
    ChatFolderEditController controller;
    controller.begin(1, QStringLiteral("Work"));
    QSignalSpy spy(&controller, &ChatFolderEditController::changed);

    controller.markSaved();
    controller.markSaved();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.dirty());
}

void ChatFolderEditControllerTest::markSavedIsIgnoredWhenInactive()
{
    ChatFolderEditController controller;
    QSignalSpy spy(&controller, &ChatFolderEditController::changed);

    controller.markSaved();
    QCOMPARE(spy.count(), 0);

    controller.begin(1, QStringLiteral("Work"));
    controller.cancel();
    controller.markSaved();

    QCOMPARE(spy.count(), 2);
    QVERIFY(!controller.active());
    QCOMPARE(controller.title(), QString());
}

void ChatFolderEditControllerTest::changedOnlyOnRealChanges()
{
    ChatFolderEditController controller;
    QSignalSpy spy(&controller, &ChatFolderEditController::changed);

    controller.begin(0, QStringLiteral("Work"));
    QCOMPARE(spy.count(), 1);

    controller.begin(0, QStringLiteral("Work"));
    QCOMPARE(spy.count(), 1);

    controller.setTitle(QStringLiteral(" Work "));
    QCOMPARE(spy.count(), 1);

    controller.setTitle(QStringLiteral("Play"));
    QCOMPARE(spy.count(), 2);

    controller.markSaved();
    QCOMPARE(spy.count(), 3);

    controller.markSaved();
    QCOMPARE(spy.count(), 3);

    controller.cancel();
    QCOMPARE(spy.count(), 4);

    controller.cancel();
    controller.setTitle(QStringLiteral("Ignored"));
    controller.markSaved();
    QCOMPARE(spy.count(), 4);
}

void ChatFolderEditControllerTest::titleMatchesFolderEditPayload()
{
    // The draft is the payload: whatever the controller holds is exactly the
    // name createChatFolder/editChatFolder would send.
    const QString inputs[] = {
        QStringLiteral("Work"),
        QStringLiteral("  Work  chats \n"),
        QStringLiteral("abcdefghijklmnop"),
        QStringLiteral("aaaaaaaaaaa") + smiley(),
        QStringLiteral("   "),
        QString(),
    };

    for (const QString& input : inputs) {
        ChatFolderEditController controller;
        controller.begin(0, input);
        QCOMPARE(controller.title(), payloadTitle(input));

        controller.setTitle(input + QStringLiteral("  tail "));
        QCOMPARE(controller.title(), payloadTitle(input + QStringLiteral("  tail ")));
    }
}

QTEST_GUILESS_MAIN(ChatFolderEditControllerTest)
#include "tst_chat_folder_edit_controller.moc"
