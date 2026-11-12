#include "telegram/ChatActivityModel.h"

#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

// The ellipsis is the single character the model paints, not three dots.
const QString kTyping = QStringLiteral("typing…");
const QString kUploading = QStringLiteral("uploading…");
const QString kRecording = QStringLiteral("recording…");

ChatActivity record(qint64 chatId)
{
    ChatActivity activity;
    activity.chatId = chatId;
    return activity;
}

} // namespace

class ChatActivityModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void emptyRecordCarriesRequestedChatId();

    void setActivityStoresEveryField();
    void setActivityRejectsInvalidChatId();
    void setActivityClampsNegativeCounts();
    void setActivityTrimsDraftText();
    void setActivityBlankDraftClearsDraft();
    void setActivityKeepsTextWithoutDraftFlag();
    void setActivityClampsNegativeReply();
    void setActivityReplacesPreviousRecord();
    void setActivityIgnoresIdenticalRecord();
    void setActivityEmptyRecordRemovesChat();

    void setTypingStoresCount();
    void countSettersClampNegatives();
    void zeroOnUnknownChatIsSilent();
    void zeroingLastCountDropsRecord();
    void kindsAreIndependent();

    void statusTextIsEmptyWithoutActivity();
    void statusTextPrioritisesRecording();
    void statusTextPrioritisesUploadingOverTyping();
    void statusTextCountsOtherMembers();
    void statusTextIgnoresDraft();

    void setDraftStoresTrimmedTextAndReply();
    void setDraftWithoutReply();
    void setDraftRejectsInvalidChatId();
    void setDraftIgnoresIdenticalContent();
    void setDraftSignalsChangedReply();
    void setDraftBlankClearsDraft();
    void setDraftBlankWithoutDraftIsSilent();
    void setDraftClampsNegativeReply();

    void clearDraftKeepsCounts();
    void clearDraftDropsEmptyRecord();
    void clearDraftWithoutDraftIsSilent();
    void clearDraftRejectsInvalidChatId();

    void hasActivityTracksCountsAndDraft();

    void removeChatForgetsEverything();
    void removeChatMissingChatIsSilent();
    void removeChatRejectsInvalidChatId();

    void clearDropsEveryChat();
    void clearOnEmptyModelIsSilent();

    void chatsAreIsolated();
    void keepsLargeChatIds();
    void changedTracksOnlyRealChanges();
};

void ChatActivityModelTest::startsEmpty()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    QVERIFY(!model.hasActivity(1));
    QVERIFY(model.statusText(1).isEmpty());
    QCOMPARE(spy.count(), 0);
}

void ChatActivityModelTest::emptyRecordCarriesRequestedChatId()
{
    ChatActivityModel model;

    const ChatActivity activity = model.activity(12345);

    // Mirrors DraftStore::draft(): the caller gets a usable record even when
    // nothing is stored, so it can bind the chat id without a special case.
    QCOMPARE(activity.chatId, qint64(12345));
    QCOMPARE(activity.typingCount, 0);
    QCOMPARE(activity.uploadingCount, 0);
    QCOMPARE(activity.recordingCount, 0);
    QVERIFY(!activity.hasDraft);
    QVERIFY(activity.draftText.isEmpty());
    QCOMPARE(activity.draftReplyTo, qint64(0));
}

void ChatActivityModelTest::setActivityStoresEveryField()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    ChatActivity incoming = record(42);
    incoming.typingCount = 3;
    incoming.uploadingCount = 1;
    incoming.recordingCount = 2;
    incoming.hasDraft = true;
    incoming.draftText = QStringLiteral("Hello");
    incoming.draftReplyTo = 77;
    model.setActivity(incoming);

    QCOMPARE(spy.count(), 1);
    const ChatActivity stored = model.activity(42);
    QCOMPARE(stored.chatId, qint64(42));
    QCOMPARE(stored.typingCount, 3);
    QCOMPARE(stored.uploadingCount, 1);
    QCOMPARE(stored.recordingCount, 2);
    QVERIFY(stored.hasDraft);
    QCOMPARE(stored.draftText, QStringLiteral("Hello"));
    QCOMPARE(stored.draftReplyTo, qint64(77));
    QVERIFY(model.hasActivity(42));
}

void ChatActivityModelTest::setActivityRejectsInvalidChatId()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    ChatActivity incoming = record(0);
    incoming.typingCount = 4;
    incoming.hasDraft = true;
    incoming.draftText = QStringLiteral("lost");
    model.setActivity(incoming);

    ChatActivity negative = record(-9);
    negative.recordingCount = 1;
    model.setActivity(negative);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.hasActivity(0));
    QVERIFY(!model.hasActivity(-9));
}

void ChatActivityModelTest::setActivityClampsNegativeCounts()
{
    ChatActivityModel model;

    ChatActivity incoming = record(5);
    incoming.typingCount = -1;
    incoming.uploadingCount = -7;
    incoming.recordingCount = -3;
    model.setActivity(incoming);

    // Every count was negative, so the normalised record has nothing to show
    // and is not stored at all.
    QVERIFY(!model.hasActivity(5));
    QCOMPARE(model.activity(5).typingCount, 0);

    incoming.typingCount = 2;
    model.setActivity(incoming);

    const ChatActivity stored = model.activity(5);
    QCOMPARE(stored.typingCount, 2);
    QCOMPARE(stored.uploadingCount, 0);
    QCOMPARE(stored.recordingCount, 0);
}

void ChatActivityModelTest::setActivityTrimsDraftText()
{
    ChatActivityModel model;

    ChatActivity incoming = record(5);
    incoming.hasDraft = true;
    incoming.draftText = QStringLiteral("  hi there \n");
    model.setActivity(incoming);

    const ChatActivity stored = model.activity(5);
    QVERIFY(stored.hasDraft);
    QCOMPARE(stored.draftText, QStringLiteral("hi there"));
}

void ChatActivityModelTest::setActivityBlankDraftClearsDraft()
{
    ChatActivityModel model;

    ChatActivity incoming = record(5);
    incoming.typingCount = 1;
    incoming.hasDraft = true;
    incoming.draftText = QStringLiteral("  \t\n");
    incoming.draftReplyTo = 11;
    model.setActivity(incoming);

    const ChatActivity stored = model.activity(5);
    QVERIFY(!stored.hasDraft);
    QVERIFY(stored.draftText.isEmpty());
    QCOMPARE(stored.draftReplyTo, qint64(0));
    QCOMPARE(stored.typingCount, 1);
}

void ChatActivityModelTest::setActivityKeepsTextWithoutDraftFlag()
{
    ChatActivityModel model;

    // A caller may leave hasDraft at its default while still handing over draft
    // text. The text wins, so the user's draft is not silently lost.
    ChatActivity incoming = record(5);
    incoming.typingCount = 1;
    incoming.hasDraft = false;
    incoming.draftText = QStringLiteral("kept");
    incoming.draftReplyTo = 8;
    model.setActivity(incoming);

    const ChatActivity stored = model.activity(5);
    QVERIFY(stored.hasDraft);
    QCOMPARE(stored.draftText, QStringLiteral("kept"));
    QCOMPARE(stored.draftReplyTo, qint64(8));
    QCOMPARE(stored.typingCount, 1);
}

void ChatActivityModelTest::setActivityClampsNegativeReply()
{
    ChatActivityModel model;

    ChatActivity incoming = record(5);
    incoming.hasDraft = true;
    incoming.draftText = QStringLiteral("reply");
    incoming.draftReplyTo = -4;
    model.setActivity(incoming);

    QCOMPARE(model.activity(5).draftReplyTo, qint64(0));
}

void ChatActivityModelTest::setActivityReplacesPreviousRecord()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setTyping(5, 3);
    model.setDraft(5, QStringLiteral("old draft"), 9);
    QCOMPARE(spy.count(), 2);

    ChatActivity incoming = record(5);
    incoming.recordingCount = 1;
    model.setActivity(incoming);

    QCOMPARE(spy.count(), 3);
    const ChatActivity stored = model.activity(5);
    QCOMPARE(stored.typingCount, 0);
    QCOMPARE(stored.recordingCount, 1);
    QVERIFY(!stored.hasDraft);
    QVERIFY(stored.draftText.isEmpty());
    QCOMPARE(stored.draftReplyTo, qint64(0));
}

void ChatActivityModelTest::setActivityIgnoresIdenticalRecord()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    ChatActivity incoming = record(5);
    incoming.typingCount = 2;
    incoming.hasDraft = true;
    incoming.draftText = QStringLiteral(" hi ");
    incoming.draftReplyTo = 9;

    model.setActivity(incoming);
    QCOMPARE(spy.count(), 1);

    // Same record with the same padding: trimming normalises it back to the
    // stored form, so nothing visible changed.
    incoming.draftText = QStringLiteral("hi");
    model.setActivity(incoming);
    model.setActivity(incoming);
    QCOMPARE(spy.count(), 1);
}

void ChatActivityModelTest::setActivityEmptyRecordRemovesChat()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setDraft(5, QStringLiteral("draft"));
    model.setTyping(5, 2);
    QCOMPARE(spy.count(), 2);

    model.setActivity(record(5));

    QCOMPARE(spy.count(), 3);
    QVERIFY(!model.hasActivity(5));
    QCOMPARE(model.activity(5).chatId, qint64(5));

    // Removing a chat that is already gone is silent.
    model.setActivity(record(5));
    QCOMPARE(spy.count(), 3);
}

void ChatActivityModelTest::setTypingStoresCount()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setTyping(7, 2);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.activity(7).typingCount, 2);
    QVERIFY(model.hasActivity(7));
    QCOMPARE(model.statusText(7), kTyping);
}

void ChatActivityModelTest::countSettersClampNegatives()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setTyping(7, 2);
    QCOMPARE(spy.count(), 1);

    // A negative count is zero, which is a real change from two.
    model.setTyping(7, -3);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.activity(7).typingCount, 0);
    QVERIFY(!model.hasActivity(7));

    model.setUploading(7, -1);
    model.setRecording(7, -5);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!model.hasActivity(7));
}

void ChatActivityModelTest::zeroOnUnknownChatIsSilent()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setTyping(7, 0);
    model.setUploading(7, 0);
    model.setRecording(7, 0);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.hasActivity(7));
}

void ChatActivityModelTest::zeroingLastCountDropsRecord()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setRecording(7, 1);
    model.setRecording(7, 0);

    QCOMPARE(spy.count(), 2);
    QVERIFY(!model.hasActivity(7));
    QVERIFY(model.statusText(7).isEmpty());
    QCOMPARE(model.activity(7).recordingCount, 0);
}

void ChatActivityModelTest::kindsAreIndependent()
{
    ChatActivityModel model;

    model.setTyping(7, 1);
    model.setUploading(7, 2);
    model.setRecording(7, 3);

    const ChatActivity stored = model.activity(7);
    QCOMPARE(stored.typingCount, 1);
    QCOMPARE(stored.uploadingCount, 2);
    QCOMPARE(stored.recordingCount, 3);

    model.setUploading(7, 0);

    QCOMPARE(model.activity(7).typingCount, 1);
    QCOMPARE(model.activity(7).uploadingCount, 0);
    QCOMPARE(model.activity(7).recordingCount, 3);
    QVERIFY(model.hasActivity(7));
}

void ChatActivityModelTest::statusTextIsEmptyWithoutActivity()
{
    ChatActivityModel model;

    QVERIFY(model.statusText(7).isEmpty());

    model.setTyping(7, 1);
    model.setTyping(7, 0);
    QVERIFY(model.statusText(7).isEmpty());
}

void ChatActivityModelTest::statusTextPrioritisesRecording()
{
    ChatActivityModel model;

    model.setTyping(7, 1);
    QCOMPARE(model.statusText(7), kTyping);

    model.setUploading(7, 1);
    QCOMPARE(model.statusText(7), kUploading);

    model.setRecording(7, 1);
    QCOMPARE(model.statusText(7), kRecording);

    // Dropping the stronger kind uncovers the next one instead of clearing.
    model.setRecording(7, 0);
    QCOMPARE(model.statusText(7), kUploading);
    model.setUploading(7, 0);
    QCOMPARE(model.statusText(7), kTyping);
}

void ChatActivityModelTest::statusTextPrioritisesUploadingOverTyping()
{
    ChatActivityModel model;

    model.setTyping(7, 4);
    model.setUploading(7, 2);

    QCOMPARE(model.statusText(7), QStringLiteral("uploading… +1"));
}

void ChatActivityModelTest::statusTextCountsOtherMembers()
{
    ChatActivityModel model;

    model.setTyping(7, 1);
    QCOMPARE(model.statusText(7), kTyping);

    model.setTyping(7, 2);
    QCOMPARE(model.statusText(7), QStringLiteral("typing… +1"));

    model.setTyping(7, 5);
    QCOMPARE(model.statusText(7), QStringLiteral("typing… +4"));

    model.setRecording(7, 3);
    QCOMPARE(model.statusText(7), QStringLiteral("recording… +2"));

    model.setUploading(7, 4);
    model.setRecording(7, 0);
    QCOMPARE(model.statusText(7), QStringLiteral("uploading… +3"));
}

void ChatActivityModelTest::statusTextIgnoresDraft()
{
    ChatActivityModel model;

    model.setDraft(7, QStringLiteral("unsent"), 3);
    QVERIFY(model.statusText(7).isEmpty());

    model.setTyping(7, 1);
    QCOMPARE(model.statusText(7), kTyping);

    model.clearDraft(7);
    QCOMPARE(model.statusText(7), kTyping);
}

void ChatActivityModelTest::setDraftStoresTrimmedTextAndReply()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setDraft(7, QStringLiteral("  draft text \n"), 55);

    QCOMPARE(spy.count(), 1);
    const ChatActivity stored = model.activity(7);
    QVERIFY(stored.hasDraft);
    QCOMPARE(stored.draftText, QStringLiteral("draft text"));
    QCOMPARE(stored.draftReplyTo, qint64(55));
    QCOMPARE(stored.typingCount, 0);
    QVERIFY(model.hasActivity(7));
}

void ChatActivityModelTest::setDraftWithoutReply()
{
    ChatActivityModel model;

    model.setDraft(7, QStringLiteral("plain"));

    const ChatActivity stored = model.activity(7);
    QVERIFY(stored.hasDraft);
    QCOMPARE(stored.draftText, QStringLiteral("plain"));
    QCOMPARE(stored.draftReplyTo, qint64(0));
}

void ChatActivityModelTest::setDraftRejectsInvalidChatId()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setDraft(0, QStringLiteral("nope"));
    model.setDraft(-3, QStringLiteral("nope"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.hasActivity(0));
    QVERIFY(!model.hasActivity(-3));
}

void ChatActivityModelTest::setDraftIgnoresIdenticalContent()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setDraft(7, QStringLiteral("same"), 4);
    QCOMPARE(spy.count(), 1);

    model.setDraft(7, QStringLiteral("  same  "), 4);
    model.setDraft(7, QStringLiteral("same"), 4);
    QCOMPARE(spy.count(), 1);
}

void ChatActivityModelTest::setDraftSignalsChangedReply()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setDraft(7, QStringLiteral("same"), 4);
    model.setDraft(7, QStringLiteral("same"), 5);

    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.activity(7).draftReplyTo, qint64(5));
}

void ChatActivityModelTest::setDraftBlankClearsDraft()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setDraft(7, QStringLiteral("text"), 4);
    QCOMPARE(spy.count(), 1);

    model.setDraft(7, QStringLiteral("   \n\t"), 9);

    QCOMPARE(spy.count(), 2);
    QVERIFY(!model.hasActivity(7));
    QVERIFY(!model.activity(7).hasDraft);
    QCOMPARE(model.activity(7).draftReplyTo, qint64(0));
}

void ChatActivityModelTest::setDraftBlankWithoutDraftIsSilent()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setDraft(7, QStringLiteral("  "));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.hasActivity(7));
}

void ChatActivityModelTest::setDraftClampsNegativeReply()
{
    ChatActivityModel model;

    model.setDraft(7, QStringLiteral("text"), -12);

    QCOMPARE(model.activity(7).draftReplyTo, qint64(0));
}

void ChatActivityModelTest::clearDraftKeepsCounts()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setTyping(7, 2);
    model.setDraft(7, QStringLiteral("draft"), 6);
    QCOMPARE(spy.count(), 2);

    model.clearDraft(7);

    QCOMPARE(spy.count(), 3);
    const ChatActivity stored = model.activity(7);
    QVERIFY(!stored.hasDraft);
    QVERIFY(stored.draftText.isEmpty());
    QCOMPARE(stored.draftReplyTo, qint64(0));
    QCOMPARE(stored.typingCount, 2);
    QVERIFY(model.hasActivity(7));
    QCOMPARE(model.statusText(7), kTyping);
}

void ChatActivityModelTest::clearDraftDropsEmptyRecord()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setDraft(7, QStringLiteral("draft"));
    model.clearDraft(7);

    QCOMPARE(spy.count(), 2);
    QVERIFY(!model.hasActivity(7));
    QVERIFY(model.statusText(7).isEmpty());
    QCOMPARE(model.activity(7).chatId, qint64(7));
}

void ChatActivityModelTest::clearDraftWithoutDraftIsSilent()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.clearDraft(7);
    QCOMPARE(spy.count(), 0);

    model.setTyping(7, 1);
    model.clearDraft(7);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.activity(7).typingCount, 1);
}

void ChatActivityModelTest::clearDraftRejectsInvalidChatId()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.clearDraft(0);
    model.clearDraft(-1);

    QCOMPARE(spy.count(), 0);
}

void ChatActivityModelTest::hasActivityTracksCountsAndDraft()
{
    ChatActivityModel model;

    QVERIFY(!model.hasActivity(7));

    model.setDraft(7, QStringLiteral("draft"));
    QVERIFY(model.hasActivity(7));

    model.clearDraft(7);
    QVERIFY(!model.hasActivity(7));

    model.setUploading(7, 1);
    QVERIFY(model.hasActivity(7));

    model.setUploading(7, 0);
    QVERIFY(!model.hasActivity(7));

    model.setRecording(7, 2);
    QVERIFY(model.hasActivity(7));

    model.setRecording(7, 0);
    QVERIFY(!model.hasActivity(7));
}

void ChatActivityModelTest::removeChatForgetsEverything()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setTyping(7, 2);
    model.setDraft(7, QStringLiteral("draft"), 3);
    QCOMPARE(spy.count(), 2);

    model.removeChat(7);

    QCOMPARE(spy.count(), 3);
    QVERIFY(!model.hasActivity(7));
    QVERIFY(model.statusText(7).isEmpty());
    QVERIFY(model.activity(7).draftText.isEmpty());
}

void ChatActivityModelTest::removeChatMissingChatIsSilent()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.removeChat(7);

    QCOMPARE(spy.count(), 0);
}

void ChatActivityModelTest::removeChatRejectsInvalidChatId()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.removeChat(0);
    model.removeChat(-8);

    QCOMPARE(spy.count(), 0);
}

void ChatActivityModelTest::clearDropsEveryChat()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setTyping(1, 1);
    model.setUploading(2, 2);
    model.setDraft(3, QStringLiteral("draft"));
    QCOMPARE(spy.count(), 3);

    model.clear();

    QCOMPARE(spy.count(), 4);
    QVERIFY(!model.hasActivity(1));
    QVERIFY(!model.hasActivity(2));
    QVERIFY(!model.hasActivity(3));
    QVERIFY(model.statusText(1).isEmpty());
}

void ChatActivityModelTest::clearOnEmptyModelIsSilent()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.clear();
    model.clear();

    QCOMPARE(spy.count(), 0);
}

void ChatActivityModelTest::chatsAreIsolated()
{
    ChatActivityModel model;

    model.setTyping(1, 2);
    model.setRecording(2, 1);
    model.setDraft(1, QStringLiteral("one"), 10);

    QCOMPARE(model.activity(1).typingCount, 2);
    QCOMPARE(model.activity(1).recordingCount, 0);
    QCOMPARE(model.activity(1).draftText, QStringLiteral("one"));
    QCOMPARE(model.activity(2).typingCount, 0);
    QCOMPARE(model.activity(2).recordingCount, 1);
    QVERIFY(!model.activity(2).hasDraft);

    model.removeChat(1);

    QVERIFY(!model.hasActivity(1));
    QVERIFY(model.hasActivity(2));
    QCOMPARE(model.activity(2).recordingCount, 1);
}

void ChatActivityModelTest::keepsLargeChatIds()
{
    ChatActivityModel model;

    // Telegram ids exceed 32 bits; the model must not truncate them.
    const qint64 bigId = Q_INT64_C(7348291037465);
    model.setTyping(bigId, 1);
    model.setDraft(bigId, QStringLiteral("draft"), Q_INT64_C(9007199254740993));

    QCOMPARE(model.activity(bigId).chatId, bigId);
    QCOMPARE(model.activity(bigId).typingCount, 1);
    QCOMPARE(model.activity(bigId).draftReplyTo, Q_INT64_C(9007199254740993));
    QVERIFY(model.hasActivity(bigId));
}

void ChatActivityModelTest::changedTracksOnlyRealChanges()
{
    ChatActivityModel model;
    QSignalSpy spy(&model, &ChatActivityModel::changed);

    model.setTyping(7, 2);                     // 1: added
    model.setTyping(7, 2);                     // silent: identical
    model.setTyping(7, 0);                     // 2: record dropped
    model.setTyping(7, 0);                     // silent: nothing stored
    model.setDraft(7, QStringLiteral("  "));   // silent: blank clears nothing
    model.setDraft(7, QStringLiteral("hi"));   // 3: draft added
    model.setDraft(7, QStringLiteral(" hi ")); // silent: trimmed identical
    model.clearDraft(7);                       // 4: draft dropped
    model.clearDraft(7);                       // silent: already clear
    model.clear();                             // silent: model empty

    QCOMPARE(spy.count(), 4);
    QVERIFY(!model.hasActivity(7));
}

QTEST_GUILESS_MAIN(ChatActivityModelTest)
#include "tst_chat_activity_model.moc"
