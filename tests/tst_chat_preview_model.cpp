#include "telegram/ChatPreviewModel.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

// The bullet is the single character the model paints for a muted chat.
const QString kDot = QStringLiteral("•");

// Built from UTF-8 bytes so the source stays ASCII and the literal is one
// surrogate pair, i.e. two UTF-16 units.
QString emoji()
{
    return QString::fromUtf8("\xF0\x9F\x98\x80");
}

ChatPreviewState previewOf(const QString& text)
{
    ChatPreviewState state;
    state.text = text;
    return state;
}

} // namespace

class ChatPreviewModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void unknownChatHasDefaultPreview();

    void setPreviewStoresEveryField();
    void setPreviewRejectsInvalidChatId();
    void setPreviewCollapsesWhitespace();
    void setPreviewCapsLongText();
    void setPreviewKeepsTextAtCap();
    void setPreviewDoesNotSplitSurrogatePair();
    void setPreviewTrimsBlankText();
    void setPreviewClampsNegativeUnread();
    void setPreviewReplacesPreviousState();
    void setPreviewIgnoresIdenticalState();
    void setPreviewEmptyStateIsNotStored();
    void setPreviewKeepsLastMessageDate();
    void markersWithoutTextAreStored();

    void badgeIsEmptyWithoutUnread();
    void badgeIgnoresNegativeUnread();
    void badgeShowsUnreadCount();
    void badgeCapsAtDefaultMax();
    void badgeRespectsCustomMax();
    void badgeClampsNonPositiveMax();
    void badgeShowsDotForMutedChat();

    void timeTextIsEmptyWithoutDate();
    void timeTextUnderAMinuteIsNow();
    void timeTextMinutes();
    void timeTextHours();
    void timeTextYesterday();
    void timeTextOlderShowsDayAndMonth();
    void timeTextFutureDateIsNow();
    void timeTextNeedsOnlyDate();

    void setLastMessageDateRejectsInvalidChatId();
    void setLastMessageDateClampsNegativeToNoDate();
    void setLastMessageDateIgnoresIdenticalDate();
    void setLastMessageDateKeepsPreview();

    void removeForgetsPreviewAndDate();
    void removeMissingChatIsSilent();
    void removeRejectsInvalidChatId();
    void clearDropsEveryChat();
    void clearOnEmptyModelIsSilent();

    void chatsAreIsolated();
    void keepsLargeChatIds();
    void changedTracksOnlyRealChanges();
};

void ChatPreviewModelTest::startsEmpty()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    QVERIFY(model.preview(1).text.isEmpty());
    QVERIFY(model.badgeText(1).isEmpty());
    QVERIFY(model.timeText(1, 1000).isEmpty());
    QCOMPARE(spy.count(), 0);
}

void ChatPreviewModelTest::unknownChatHasDefaultPreview()
{
    ChatPreviewModel model;

    const qint64 ids[] = {0, -1, 999};
    for (const qint64 chatId : ids) {
        const ChatPreviewState state = model.preview(chatId);
        QVERIFY(state.text.isEmpty());
        QVERIFY(!state.isTyping);
        QCOMPARE(state.unreadCount, 0);
        QVERIFY(!state.muted);
        QVERIFY(!state.pinned);
        QVERIFY(!state.draft);
    }
}

void ChatPreviewModelTest::setPreviewStoresEveryField()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    ChatPreviewState incoming;
    incoming.text = QStringLiteral("Hello there");
    incoming.isTyping = true;
    incoming.unreadCount = 7;
    incoming.muted = true;
    incoming.pinned = true;
    incoming.draft = true;
    model.setPreview(42, incoming);

    QCOMPARE(spy.count(), 1);
    const ChatPreviewState stored = model.preview(42);
    QCOMPARE(stored.text, QStringLiteral("Hello there"));
    QVERIFY(stored.isTyping);
    QCOMPARE(stored.unreadCount, 7);
    QVERIFY(stored.muted);
    QVERIFY(stored.pinned);
    QVERIFY(stored.draft);
}

void ChatPreviewModelTest::setPreviewRejectsInvalidChatId()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    model.setPreview(0, previewOf(QStringLiteral("lost")));
    model.setPreview(-9, previewOf(QStringLiteral("lost")));

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.preview(0).text.isEmpty());
    QVERIFY(model.preview(-9).text.isEmpty());
}

void ChatPreviewModelTest::setPreviewCollapsesWhitespace()
{
    ChatPreviewModel model;

    // A row paints one line, so every run of whitespace, including the
    // newlines a multi-line message carries, becomes a single space.
    model.setPreview(5, previewOf(QStringLiteral("  Hello\n\nworld\tfrom   BeyGram  ")));
    QCOMPARE(model.preview(5).text, QStringLiteral("Hello world from BeyGram"));

    model.setPreview(6, previewOf(QStringLiteral("first\nsecond")));
    QCOMPARE(model.preview(6).text, QStringLiteral("first second"));
}

void ChatPreviewModelTest::setPreviewCapsLongText()
{
    ChatPreviewModel model;

    model.setPreview(5, previewOf(QString(250, QLatin1Char('x'))));

    QCOMPARE(model.preview(5).text.size(), 200);
    QCOMPARE(model.preview(5).text, QString(200, QLatin1Char('x')));
}

void ChatPreviewModelTest::setPreviewKeepsTextAtCap()
{
    ChatPreviewModel model;

    const QString exactly = QString(200, QLatin1Char('y'));
    model.setPreview(5, previewOf(exactly));

    QCOMPARE(model.preview(5).text, exactly);
}

void ChatPreviewModelTest::setPreviewDoesNotSplitSurrogatePair()
{
    ChatPreviewModel model;

    // 199 units plus the emoji's two units is 201. Cutting at 200 would leave
    // the emoji's high surrogate alone, which cannot be painted, so the model
    // drops the whole pair instead.
    const QString text = QString(199, QLatin1Char('a')) + emoji();
    QCOMPARE(text.size(), 201);

    model.setPreview(5, previewOf(text));

    const QString stored = model.preview(5).text;
    QCOMPARE(stored.size(), 199);
    QCOMPARE(stored, QString(199, QLatin1Char('a')));
    QVERIFY(!stored.at(stored.size() - 1).isHighSurrogate());
}

void ChatPreviewModelTest::setPreviewTrimsBlankText()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    // Whitespace-only text normalises to nothing, so there is no preview to
    // store and nothing a row could paint.
    model.setPreview(5, previewOf(QStringLiteral("  \n\t  ")));

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.preview(5).text.isEmpty());
}

void ChatPreviewModelTest::setPreviewClampsNegativeUnread()
{
    ChatPreviewModel model;

    ChatPreviewState incoming = previewOf(QStringLiteral("hi"));
    incoming.unreadCount = -4;
    model.setPreview(5, incoming);

    QCOMPARE(model.preview(5).unreadCount, 0);
    QVERIFY(model.badgeText(5).isEmpty());
}

void ChatPreviewModelTest::setPreviewReplacesPreviousState()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    ChatPreviewState first;
    first.text = QStringLiteral("first");
    first.isTyping = true;
    first.unreadCount = 4;
    first.muted = true;
    first.pinned = true;
    first.draft = true;
    model.setPreview(5, first);

    // setPreview() replaces the record; it does not merge into the old one.
    model.setPreview(5, previewOf(QStringLiteral("second")));

    QCOMPARE(spy.count(), 2);
    const ChatPreviewState stored = model.preview(5);
    QCOMPARE(stored.text, QStringLiteral("second"));
    QVERIFY(!stored.isTyping);
    QCOMPARE(stored.unreadCount, 0);
    QVERIFY(!stored.muted);
    QVERIFY(!stored.pinned);
    QVERIFY(!stored.draft);
}

void ChatPreviewModelTest::setPreviewIgnoresIdenticalState()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    model.setPreview(5, previewOf(QStringLiteral(" hi  there ")));
    QCOMPARE(spy.count(), 1);

    // The padding normalises away, so nothing a row paints moved.
    model.setPreview(5, previewOf(QStringLiteral("hi there")));
    QCOMPARE(spy.count(), 1);
}

void ChatPreviewModelTest::setPreviewEmptyStateIsNotStored()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    model.setPreview(5, ChatPreviewState{});
    QCOMPARE(spy.count(), 0);
    QVERIFY(model.preview(5).text.isEmpty());

    model.setPreview(5, previewOf(QStringLiteral("hi")));
    QCOMPARE(spy.count(), 1);

    model.setPreview(5, ChatPreviewState{});
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.preview(5).text.isEmpty());
}

void ChatPreviewModelTest::setPreviewKeepsLastMessageDate()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    const qint64 now = Q_INT64_C(1700000000);
    model.setLastMessageDate(5, now - 120);
    QCOMPARE(spy.count(), 1);

    model.setPreview(5, previewOf(QStringLiteral("hi")));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.timeText(5, now), QStringLiteral("2m"));

    // Clearing the preview must not clear the date: the row still shows when
    // the last message arrived.
    model.setPreview(5, ChatPreviewState{});
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.timeText(5, now), QStringLiteral("2m"));
}

void ChatPreviewModelTest::markersWithoutTextAreStored()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    ChatPreviewState typing;
    typing.isTyping = true;
    model.setPreview(1, typing);

    ChatPreviewState pinned;
    pinned.pinned = true;
    model.setPreview(2, pinned);

    QCOMPARE(spy.count(), 2);
    QVERIFY(model.preview(1).isTyping);
    QVERIFY(model.preview(2).pinned);
    QVERIFY(model.badgeText(1).isEmpty());
    QVERIFY(model.timeText(1, 1000).isEmpty());
}

void ChatPreviewModelTest::badgeIsEmptyWithoutUnread()
{
    ChatPreviewModel model;

    QVERIFY(model.badgeText(5).isEmpty());

    model.setPreview(5, previewOf(QStringLiteral("hi")));
    QVERIFY(model.badgeText(5).isEmpty());

    ChatPreviewState muted = previewOf(QStringLiteral("hi"));
    muted.muted = true;
    model.setPreview(6, muted);
    QVERIFY(model.badgeText(6).isEmpty());
}

void ChatPreviewModelTest::badgeIgnoresNegativeUnread()
{
    ChatPreviewModel model;

    ChatPreviewState incoming = previewOf(QStringLiteral("hi"));
    incoming.unreadCount = -12;
    model.setPreview(5, incoming);

    QVERIFY(model.badgeText(5).isEmpty());
    QCOMPARE(model.preview(5).unreadCount, 0);
}

void ChatPreviewModelTest::badgeShowsUnreadCount()
{
    ChatPreviewModel model;

    ChatPreviewState incoming = previewOf(QStringLiteral("hi"));
    incoming.unreadCount = 7;
    model.setPreview(5, incoming);

    QCOMPARE(model.badgeText(5), QStringLiteral("7"));
    QCOMPARE(model.badgeText(5, 10), QStringLiteral("7"));
}

void ChatPreviewModelTest::badgeCapsAtDefaultMax()
{
    ChatPreviewModel model;

    ChatPreviewState incoming = previewOf(QStringLiteral("hi"));
    incoming.unreadCount = 99;
    model.setPreview(5, incoming);
    QCOMPARE(model.badgeText(5), QStringLiteral("99"));

    incoming.unreadCount = 100;
    model.setPreview(5, incoming);
    QCOMPARE(model.badgeText(5), QStringLiteral("99+"));

    incoming.unreadCount = 1000000;
    model.setPreview(5, incoming);
    QCOMPARE(model.badgeText(5), QStringLiteral("99+"));
}

void ChatPreviewModelTest::badgeRespectsCustomMax()
{
    ChatPreviewModel model;

    ChatPreviewState incoming = previewOf(QStringLiteral("hi"));
    incoming.unreadCount = 5;
    model.setPreview(5, incoming);
    QCOMPARE(model.badgeText(5, 5), QStringLiteral("5"));

    incoming.unreadCount = 6;
    model.setPreview(5, incoming);
    QCOMPARE(model.badgeText(5, 5), QStringLiteral("5+"));

    incoming.unreadCount = 2;
    model.setPreview(5, incoming);
    QCOMPARE(model.badgeText(5, 1), QStringLiteral("1+"));
}

void ChatPreviewModelTest::badgeClampsNonPositiveMax()
{
    ChatPreviewModel model;

    ChatPreviewState incoming = previewOf(QStringLiteral("hi"));
    incoming.unreadCount = 1;
    model.setPreview(5, incoming);
    QCOMPARE(model.badgeText(5, 0), QStringLiteral("1"));

    incoming.unreadCount = 2;
    model.setPreview(5, incoming);
    // "0+" would name no count, so the smallest limit is one.
    QCOMPARE(model.badgeText(5, 0), QStringLiteral("1+"));
    QCOMPARE(model.badgeText(5, -7), QStringLiteral("1+"));
}

void ChatPreviewModelTest::badgeShowsDotForMutedChat()
{
    ChatPreviewModel model;

    ChatPreviewState incoming = previewOf(QStringLiteral("hi"));
    incoming.unreadCount = 42;
    incoming.muted = true;
    model.setPreview(5, incoming);

    QCOMPARE(model.badgeText(5), kDot);
    QCOMPARE(model.badgeText(5, 1), kDot);
}

void ChatPreviewModelTest::timeTextIsEmptyWithoutDate()
{
    ChatPreviewModel model;
    const qint64 now = Q_INT64_C(1700000000);

    QVERIFY(model.timeText(5, now).isEmpty());

    model.setPreview(5, previewOf(QStringLiteral("hi")));
    QVERIFY(model.timeText(5, now).isEmpty());
}

void ChatPreviewModelTest::timeTextUnderAMinuteIsNow()
{
    ChatPreviewModel model;
    const qint64 now = Q_INT64_C(1700000000);

    model.setLastMessageDate(5, now);
    QCOMPARE(model.timeText(5, now), QStringLiteral("now"));

    model.setLastMessageDate(5, now - 59);
    QCOMPARE(model.timeText(5, now), QStringLiteral("now"));
}

void ChatPreviewModelTest::timeTextMinutes()
{
    ChatPreviewModel model;
    const qint64 now = Q_INT64_C(1700000000);

    model.setLastMessageDate(5, now - 60);
    QCOMPARE(model.timeText(5, now), QStringLiteral("1m"));

    model.setLastMessageDate(5, now - 119);
    QCOMPARE(model.timeText(5, now), QStringLiteral("1m"));

    model.setLastMessageDate(5, now - 3599);
    QCOMPARE(model.timeText(5, now), QStringLiteral("59m"));
}

void ChatPreviewModelTest::timeTextHours()
{
    ChatPreviewModel model;
    const qint64 now = Q_INT64_C(1700000000);

    model.setLastMessageDate(5, now - 3600);
    QCOMPARE(model.timeText(5, now), QStringLiteral("1h"));

    model.setLastMessageDate(5, now - 7200);
    QCOMPARE(model.timeText(5, now), QStringLiteral("2h"));

    model.setLastMessageDate(5, now - 86399);
    QCOMPARE(model.timeText(5, now), QStringLiteral("23h"));
}

void ChatPreviewModelTest::timeTextYesterday()
{
    ChatPreviewModel model;
    const qint64 now = Q_INT64_C(1700000000);

    model.setLastMessageDate(5, now - 86400);
    QCOMPARE(model.timeText(5, now), QStringLiteral("Yesterday"));

    model.setLastMessageDate(5, now - 172799);
    QCOMPARE(model.timeText(5, now), QStringLiteral("Yesterday"));
}

void ChatPreviewModelTest::timeTextOlderShowsDayAndMonth()
{
    ChatPreviewModel model;
    const qint64 now = Q_INT64_C(1700000000);

    // Two days is the first "dd.MM"; five years is still "dd.MM", because the
    // compact label never carries a year.
    const qint64 ages[] = {Q_INT64_C(172800), Q_INT64_C(157680000)}; // two days, five years
    for (const qint64 age : ages) {
        const qint64 date = now - age;
        model.setLastMessageDate(5, date);

        const QString expected = QDateTime::fromSecsSinceEpoch(date)
                                     .toLocalTime()
                                     .toString(QStringLiteral("dd.MM"));
        QCOMPARE(model.timeText(5, now), expected);
        QCOMPARE(expected.size(), 5);
    }
}

void ChatPreviewModelTest::timeTextFutureDateIsNow()
{
    ChatPreviewModel model;
    const qint64 now = Q_INT64_C(1700000000);

    model.setLastMessageDate(5, now + 3600);

    // Clock skew must not produce a negative age or a huge number of hours.
    QCOMPARE(model.timeText(5, now), QStringLiteral("now"));
}

void ChatPreviewModelTest::timeTextNeedsOnlyDate()
{
    ChatPreviewModel model;
    const qint64 now = Q_INT64_C(1700000000);

    model.setLastMessageDate(5, now - 7200);

    QCOMPARE(model.timeText(5, now), QStringLiteral("2h"));
    QVERIFY(model.preview(5).text.isEmpty());
    QVERIFY(model.badgeText(5).isEmpty());
}

void ChatPreviewModelTest::setLastMessageDateRejectsInvalidChatId()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    model.setLastMessageDate(0, 1000);
    model.setLastMessageDate(-2, 1000);

    QCOMPARE(spy.count(), 0);
}

void ChatPreviewModelTest::setLastMessageDateClampsNegativeToNoDate()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);
    const qint64 now = Q_INT64_C(1700000000);

    model.setPreview(5, previewOf(QStringLiteral("hi")));
    model.setLastMessageDate(5, now - 60);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.timeText(5, now), QStringLiteral("1m"));

    // A non-positive date means "no date", not the epoch.
    model.setLastMessageDate(5, -50);
    QCOMPARE(spy.count(), 3);
    QVERIFY(model.timeText(5, now).isEmpty());
    QCOMPARE(model.preview(5).text, QStringLiteral("hi"));
}

void ChatPreviewModelTest::setLastMessageDateIgnoresIdenticalDate()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    model.setLastMessageDate(5, 1000);
    QCOMPARE(spy.count(), 1);

    model.setLastMessageDate(5, 1000);
    QCOMPARE(spy.count(), 1);
}

void ChatPreviewModelTest::setLastMessageDateKeepsPreview()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    ChatPreviewState incoming = previewOf(QStringLiteral("hi"));
    incoming.unreadCount = 3;
    model.setPreview(5, incoming);
    model.setLastMessageDate(5, 1000);

    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.preview(5).text, QStringLiteral("hi"));
    QCOMPARE(model.preview(5).unreadCount, 3);
    QCOMPARE(model.badgeText(5), QStringLiteral("3"));
}

void ChatPreviewModelTest::removeForgetsPreviewAndDate()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);
    const qint64 now = Q_INT64_C(1700000000);

    ChatPreviewState incoming = previewOf(QStringLiteral("hi"));
    incoming.unreadCount = 2;
    model.setPreview(5, incoming);
    model.setLastMessageDate(5, now - 3600);
    QCOMPARE(spy.count(), 2);

    model.remove(5);

    QCOMPARE(spy.count(), 3);
    QVERIFY(model.preview(5).text.isEmpty());
    QCOMPARE(model.preview(5).unreadCount, 0);
    QVERIFY(model.badgeText(5).isEmpty());
    QVERIFY(model.timeText(5, now).isEmpty());
}

void ChatPreviewModelTest::removeMissingChatIsSilent()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    model.remove(5);

    QCOMPARE(spy.count(), 0);
}

void ChatPreviewModelTest::removeRejectsInvalidChatId()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    model.remove(0);
    model.remove(-4);

    QCOMPARE(spy.count(), 0);
}

void ChatPreviewModelTest::clearDropsEveryChat()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    model.setPreview(1, previewOf(QStringLiteral("one")));
    model.setPreview(2, previewOf(QStringLiteral("two")));
    model.setLastMessageDate(3, 1000);
    QCOMPARE(spy.count(), 3);

    model.clear();

    QCOMPARE(spy.count(), 4);
    QVERIFY(model.preview(1).text.isEmpty());
    QVERIFY(model.preview(2).text.isEmpty());
    QVERIFY(model.timeText(3, 2000).isEmpty());
}

void ChatPreviewModelTest::clearOnEmptyModelIsSilent()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    model.clear();
    model.clear();

    QCOMPARE(spy.count(), 0);
}

void ChatPreviewModelTest::chatsAreIsolated()
{
    ChatPreviewModel model;
    const qint64 now = Q_INT64_C(1700000000);

    ChatPreviewState first;
    first.text = QStringLiteral("one");
    first.unreadCount = 4;
    first.pinned = true;
    model.setPreview(1, first);
    model.setLastMessageDate(1, now - 60);

    ChatPreviewState second;
    second.text = QStringLiteral("two");
    second.muted = true;
    model.setPreview(2, second);

    QCOMPARE(model.preview(1).text, QStringLiteral("one"));
    QCOMPARE(model.preview(1).unreadCount, 4);
    QVERIFY(model.preview(1).pinned);
    QCOMPARE(model.timeText(1, now), QStringLiteral("1m"));

    QCOMPARE(model.preview(2).text, QStringLiteral("two"));
    QVERIFY(model.preview(2).muted);
    QCOMPARE(model.preview(2).unreadCount, 0);
    QVERIFY(model.timeText(2, now).isEmpty());

    model.remove(1);

    QVERIFY(model.preview(1).text.isEmpty());
    QCOMPARE(model.preview(2).text, QStringLiteral("two"));
}

void ChatPreviewModelTest::keepsLargeChatIds()
{
    ChatPreviewModel model;
    const qint64 bigId = Q_INT64_C(7348291037465);
    const qint64 now = Q_INT64_C(1700000000);

    ChatPreviewState incoming = previewOf(QStringLiteral("big"));
    incoming.unreadCount = 8;
    model.setPreview(bigId, incoming);
    model.setLastMessageDate(bigId, now - 3600);

    QCOMPARE(model.preview(bigId).text, QStringLiteral("big"));
    QCOMPARE(model.preview(bigId).unreadCount, 8);
    QCOMPARE(model.badgeText(bigId), QStringLiteral("8"));
    QCOMPARE(model.timeText(bigId, now), QStringLiteral("1h"));
}

void ChatPreviewModelTest::changedTracksOnlyRealChanges()
{
    ChatPreviewModel model;
    QSignalSpy spy(&model, &ChatPreviewModel::changed);

    model.setPreview(7, previewOf(QStringLiteral("hi")));   // 1: added
    model.setPreview(7, previewOf(QStringLiteral(" hi "))); // silent: same line
    model.setLastMessageDate(7, 1000);                      // 2: date added
    model.setLastMessageDate(7, 1000);                      // silent: same date
    model.setPreview(7, ChatPreviewState{});                // 3: preview cleared
    model.setPreview(7, ChatPreviewState{});                // silent: already clear
    model.setLastMessageDate(7, 0);                         // 4: entry dropped
    model.setLastMessageDate(7, 0);                         // silent: nothing stored
    model.remove(7);                                        // silent: already gone
    model.clear();                                          // silent: model empty

    QCOMPARE(spy.count(), 4);
    QVERIFY(model.preview(7).text.isEmpty());
    QVERIFY(model.timeText(7, 2000).isEmpty());
}

QTEST_GUILESS_MAIN(ChatPreviewModelTest)
#include "tst_chat_preview_model.moc"
