#include "telegram/ChatSearchIndex.h"

#include <QSignalSpy>
#include <QtTest>

using beygram::telegram::ChatSearchIndex;

class ChatSearchIndexTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void ignoresNonPositiveChatId();
    void countTracksUpsertsAndRemovals();
    void normalizesStoredText();
    void matchesQueryCaseInsensitively();
    void trimsQuery();
    void emptyQueryReturnsAllNewestFirst();
    void whitespaceOnlyQueryIsEmpty();
    void emptyQueryClampsLimit();
    void emptyQueryDefaultsToThirty();
    void emptyQueryHonoursMaximumLimit();
    void matchesTitleSubstring();
    void matchesPreviewSubstring();
    void querySpanningWordsMatches();
    void nonMatchingQueryReturnsEmpty();
    void emptyTextMatchesOnlyEmptyQuery();
    void titleHitOutranksPreviewHit();
    void titlePrefixOutranksTitleSubstring();
    void previewPrefixOutranksPreviewSubstring();
    void titleSubstringOutranksPreviewPrefix();
    void normalizedPrefixIsStillAPrefix();
    void equalScoresReadNewestFirst();
    void limitAppliesToRankedResults();
    void upsertNewChatEmitsChanged();
    void upsertChangedTitleEmitsChanged();
    void upsertChangedPreviewEmitsChanged();
    void upsertMovesChatToFront();
    void reupsertNewestUnchangedIsSilent();
    void reupsertOlderUnchangedMovesToFront();
    void removeDropsOnlyThatChat();
    void removeMissingIsNoOp();
    void removeNonPositiveIsNoOp();
    void removeThenUpsertAddsAgain();
    void clearEmptiesIndex();
    void clearOnEmptyIsNoOp();
    void searchDoesNotEmitChanged();
    void changedOnlyOnRealChanges();
};

void ChatSearchIndexTest::startsEmpty()
{
    ChatSearchIndex index;

    QCOMPARE(index.count(), 0);
    QVERIFY(index.search(QString()).isEmpty());
    QVERIFY(index.search(QStringLiteral("anything")).isEmpty());
    QVERIFY(index.search(QString(), 0).isEmpty());
}

void ChatSearchIndexTest::ignoresNonPositiveChatId()
{
    ChatSearchIndex index;
    QSignalSpy spy(&index, &ChatSearchIndex::changed);

    index.upsert(0, QStringLiteral("Zero"), QStringLiteral("preview"));
    index.upsert(-1, QStringLiteral("Negative"), QStringLiteral("preview"));

    QCOMPARE(index.count(), 0);
    QVERIFY(index.search(QString()).isEmpty());
    QVERIFY(index.search(QStringLiteral("zero")).isEmpty());
    QCOMPARE(spy.count(), 0);
}

void ChatSearchIndexTest::countTracksUpsertsAndRemovals()
{
    ChatSearchIndex index;

    index.upsert(1, QStringLiteral("Alpha"), QStringLiteral("one"));
    index.upsert(2, QStringLiteral("Beta"), QStringLiteral("two"));
    QCOMPARE(index.count(), 2);

    index.upsert(2, QStringLiteral("Beta renamed"), QStringLiteral("two"));
    QCOMPARE(index.count(), 2);

    index.remove(1);
    QCOMPARE(index.count(), 1);

    index.remove(2);
    QCOMPARE(index.count(), 0);
}

void ChatSearchIndexTest::normalizesStoredText()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("  Alice  "), QStringLiteral("  Hello World  "));

    const QVector<qint64> expected{1};
    QCOMPARE(index.search(QStringLiteral("alice")), expected);
    QCOMPARE(index.search(QStringLiteral("hello world")), expected);
    QCOMPARE(index.search(QStringLiteral("world")), expected);
}

void ChatSearchIndexTest::matchesQueryCaseInsensitively()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alice"), QStringLiteral("See You At Six"));

    const QVector<qint64> expected{1};
    QCOMPARE(index.search(QStringLiteral("ALICE")), expected);
    QCOMPARE(index.search(QStringLiteral("aLiCe")), expected);
    QCOMPARE(index.search(QStringLiteral("SEE YOU")), expected);
}

void ChatSearchIndexTest::trimsQuery()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alice"), QStringLiteral("hello"));

    const QVector<qint64> expected{1};
    QCOMPARE(index.search(QStringLiteral("  alice  ")), expected);
    QCOMPARE(index.search(QStringLiteral("\t Alice \n")), expected);
    QCOMPARE(index.search(QStringLiteral("\r\nhello\t")), expected);
}

void ChatSearchIndexTest::emptyQueryReturnsAllNewestFirst()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alpha"), QString());
    index.upsert(2, QStringLiteral("Beta"), QString());
    index.upsert(3, QStringLiteral("Gamma"), QString());

    const QVector<qint64> expected{3, 2, 1};
    QCOMPARE(index.search(QString()), expected);
    QCOMPARE(index.search(QString(), 10), expected);
}

void ChatSearchIndexTest::whitespaceOnlyQueryIsEmpty()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alpha"), QString());
    index.upsert(2, QStringLiteral("Beta"), QString());

    const QVector<qint64> expected{2, 1};
    QCOMPARE(index.search(QStringLiteral("   ")), expected);
    QCOMPARE(index.search(QStringLiteral("\t\r\n")), expected);
}

void ChatSearchIndexTest::emptyQueryClampsLimit()
{
    ChatSearchIndex index;
    for (int i = 1; i <= 5; ++i)
        index.upsert(i, QStringLiteral("Chat %1").arg(i), QString());

    const QVector<qint64> one{5};
    QCOMPARE(index.search(QString(), 0), one);
    QCOMPARE(index.search(QString(), -10), one);
    QCOMPARE(index.search(QString(), 1), one);

    const QVector<qint64> three{5, 4, 3};
    QCOMPARE(index.search(QString(), 3), three);

    const QVector<qint64> all{5, 4, 3, 2, 1};
    QCOMPARE(index.search(QString(), 5), all);
    QCOMPARE(index.search(QString(), 100), all);
}

void ChatSearchIndexTest::emptyQueryDefaultsToThirty()
{
    ChatSearchIndex index;
    for (int i = 1; i <= 40; ++i)
        index.upsert(i, QStringLiteral("Chat %1").arg(i), QString());

    const QVector<qint64> results = index.search(QString());
    QCOMPARE(int(results.size()), 30);
    QCOMPARE(results.first(), qint64(40));
    QCOMPARE(results.last(), qint64(11));
}

void ChatSearchIndexTest::emptyQueryHonoursMaximumLimit()
{
    ChatSearchIndex index;
    for (int i = 1; i <= 120; ++i)
        index.upsert(i, QStringLiteral("Chat %1").arg(i), QString());

    const QVector<qint64> results = index.search(QString(), 1000);
    QCOMPARE(int(results.size()), 100);
    QCOMPARE(results.first(), qint64(120));
    QCOMPARE(results.last(), qint64(21));

    QCOMPARE(int(index.search(QString(), 100).size()), 100);
}

void ChatSearchIndexTest::matchesTitleSubstring()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Project Alice"), QStringLiteral("nothing"));

    const QVector<qint64> expected{1};
    QCOMPARE(index.search(QStringLiteral("alice")), expected);
    QCOMPARE(index.search(QStringLiteral("ject")), expected);
}

void ChatSearchIndexTest::matchesPreviewSubstring()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alice"), QStringLiteral("see you at six"));

    const QVector<qint64> expected{1};
    QCOMPARE(index.search(QStringLiteral("you at")), expected);
    QCOMPARE(index.search(QStringLiteral("six")), expected);
}

void ChatSearchIndexTest::querySpanningWordsMatches()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Weekly Team Meeting"), QString());

    const QVector<qint64> expected{1};
    QCOMPARE(index.search(QStringLiteral("team meeting")), expected);
    QCOMPARE(index.search(QStringLiteral("eekly tea")), expected);
}

void ChatSearchIndexTest::nonMatchingQueryReturnsEmpty()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alice"), QStringLiteral("hello"));

    QVERIFY(index.search(QStringLiteral("bob")).isEmpty());
    QVERIFY(index.search(QStringLiteral("alice hello")).isEmpty());
}

void ChatSearchIndexTest::emptyTextMatchesOnlyEmptyQuery()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("   "), QStringLiteral("\t\n"));

    QCOMPARE(index.count(), 1);
    const QVector<qint64> expected{1};
    QCOMPARE(index.search(QString()), expected);
    QVERIFY(index.search(QStringLiteral("a")).isEmpty());
}

void ChatSearchIndexTest::titleHitOutranksPreviewHit()
{
    ChatSearchIndex index;
    // The title hit is older; the newer preview hit must still rank second.
    index.upsert(2, QStringLiteral("Alice"), QStringLiteral("nothing"));
    index.upsert(1, QStringLiteral("nothing"), QStringLiteral("alice in the preview"));

    const QVector<qint64> expected{2, 1};
    QCOMPARE(index.search(QStringLiteral("alice")), expected);
}

void ChatSearchIndexTest::titlePrefixOutranksTitleSubstring()
{
    ChatSearchIndex index;
    // The prefix is older; the newer substring must still rank second.
    index.upsert(2, QStringLiteral("alicexx"), QString());
    index.upsert(1, QStringLiteral("xxalice"), QString());

    const QVector<qint64> expected{2, 1};
    QCOMPARE(index.search(QStringLiteral("alice")), expected);
}

void ChatSearchIndexTest::previewPrefixOutranksPreviewSubstring()
{
    ChatSearchIndex index;
    // The prefix is older; the newer substring must still rank second.
    index.upsert(2, QString(), QStringLiteral("alicexx"));
    index.upsert(1, QString(), QStringLiteral("xxalice"));

    const QVector<qint64> expected{2, 1};
    QCOMPARE(index.search(QStringLiteral("alice")), expected);
}

void ChatSearchIndexTest::titleSubstringOutranksPreviewPrefix()
{
    ChatSearchIndex index;
    // A title substring outranks a preview prefix, even though the preview hit
    // is newer.
    index.upsert(2, QString(), QStringLiteral("alicexx"));
    index.upsert(1, QStringLiteral("xxalice"), QString());

    const QVector<qint64> expected{1, 2};
    QCOMPARE(index.search(QStringLiteral("alice")), expected);
}

void ChatSearchIndexTest::normalizedPrefixIsStillAPrefix()
{
    ChatSearchIndex index;
    index.upsert(2, QStringLiteral("ALICE and Bob"), QString());
    index.upsert(1, QStringLiteral("xx alice"), QString());

    const QVector<qint64> expected{2, 1};
    QCOMPARE(index.search(QStringLiteral("aLi")), expected);
}

void ChatSearchIndexTest::equalScoresReadNewestFirst()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("xxalice"), QString());
    index.upsert(2, QStringLiteral("xxalice"), QString());
    index.upsert(3, QStringLiteral("xxalice"), QString());

    const QVector<qint64> expected{3, 2, 1};
    QCOMPARE(index.search(QStringLiteral("alice")), expected);
}

void ChatSearchIndexTest::limitAppliesToRankedResults()
{
    ChatSearchIndex index;
    index.upsert(2, QStringLiteral("Alice"), QString());
    index.upsert(1, QString(), QStringLiteral("alice"));

    // The older title hit is the better match, so it must survive a limit of 1.
    const QVector<qint64> expected{2};
    QCOMPARE(index.search(QStringLiteral("alice"), 1), expected);
}

void ChatSearchIndexTest::upsertNewChatEmitsChanged()
{
    ChatSearchIndex index;
    QSignalSpy spy(&index, &ChatSearchIndex::changed);

    index.upsert(1, QStringLiteral("Alpha"), QStringLiteral("one"));
    QCOMPARE(index.count(), 1);
    QCOMPARE(spy.count(), 1);

    index.upsert(2, QStringLiteral("Beta"), QStringLiteral("two"));
    QCOMPARE(index.count(), 2);
    QCOMPARE(spy.count(), 2);
}

void ChatSearchIndexTest::upsertChangedTitleEmitsChanged()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alice"), QStringLiteral("hello"));

    QSignalSpy spy(&index, &ChatSearchIndex::changed);
    index.upsert(1, QStringLiteral("Alicia"), QStringLiteral("hello"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(index.count(), 1);
    const QVector<qint64> expected{1};
    QCOMPARE(index.search(QStringLiteral("alicia")), expected);
    QVERIFY(index.search(QStringLiteral("alice")).isEmpty());
}

void ChatSearchIndexTest::upsertChangedPreviewEmitsChanged()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alice"), QStringLiteral("one"));

    QSignalSpy spy(&index, &ChatSearchIndex::changed);
    index.upsert(1, QStringLiteral("Alice"), QStringLiteral("two"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(index.count(), 1);
    const QVector<qint64> expected{1};
    QCOMPARE(index.search(QStringLiteral("two")), expected);
    QVERIFY(index.search(QStringLiteral("one")).isEmpty());
}

void ChatSearchIndexTest::upsertMovesChatToFront()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alpha"), QString());
    index.upsert(2, QStringLiteral("Beta"), QString());
    index.upsert(3, QStringLiteral("Gamma"), QString());

    index.upsert(1, QStringLiteral("Alpha updated"), QString());

    const QVector<qint64> expected{1, 3, 2};
    QCOMPARE(index.search(QString()), expected);
    QCOMPARE(index.count(), 3);
}

void ChatSearchIndexTest::reupsertNewestUnchangedIsSilent()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alpha"), QStringLiteral("one"));
    index.upsert(2, QStringLiteral("Beta"), QStringLiteral("two"));
    index.upsert(1, QStringLiteral("Alpha"), QStringLiteral("one")); // now newest

    QSignalSpy spy(&index, &ChatSearchIndex::changed);
    index.upsert(1, QStringLiteral("  alpha  "), QStringLiteral("  ONE  "));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(index.count(), 2);
    const QVector<qint64> expected{1, 2};
    QCOMPARE(index.search(QString()), expected);
}

void ChatSearchIndexTest::reupsertOlderUnchangedMovesToFront()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alpha"), QStringLiteral("one"));
    index.upsert(2, QStringLiteral("Beta"), QStringLiteral("two"));
    index.upsert(3, QStringLiteral("Gamma"), QStringLiteral("three"));

    QSignalSpy spy(&index, &ChatSearchIndex::changed);
    index.upsert(1, QStringLiteral(" alpha "), QStringLiteral(" one "));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(index.count(), 3);
    const QVector<qint64> expected{1, 3, 2};
    QCOMPARE(index.search(QString()), expected);
}

void ChatSearchIndexTest::removeDropsOnlyThatChat()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alpha"), QString());
    index.upsert(2, QStringLiteral("Beta"), QString());
    index.upsert(3, QStringLiteral("Gamma"), QString());

    QSignalSpy spy(&index, &ChatSearchIndex::changed);
    index.remove(2);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(index.count(), 2);
    QVERIFY(index.search(QStringLiteral("beta")).isEmpty());
    const QVector<qint64> expected{3, 1};
    QCOMPARE(index.search(QString()), expected);
}

void ChatSearchIndexTest::removeMissingIsNoOp()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alpha"), QString());

    QSignalSpy spy(&index, &ChatSearchIndex::changed);
    index.remove(99);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(index.count(), 1);
}

void ChatSearchIndexTest::removeNonPositiveIsNoOp()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alpha"), QString());

    QSignalSpy spy(&index, &ChatSearchIndex::changed);
    index.remove(0);
    index.remove(-7);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(index.count(), 1);
}

void ChatSearchIndexTest::removeThenUpsertAddsAgain()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alpha"), QStringLiteral("one"));
    index.remove(1);

    QSignalSpy spy(&index, &ChatSearchIndex::changed);
    index.upsert(1, QStringLiteral("Alpha"), QStringLiteral("one"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(index.count(), 1);
    const QVector<qint64> expected{1};
    QCOMPARE(index.search(QString()), expected);
    QCOMPARE(index.search(QStringLiteral("alpha")), expected);
}

void ChatSearchIndexTest::clearEmptiesIndex()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alpha"), QString());
    index.upsert(2, QStringLiteral("Beta"), QString());

    QSignalSpy spy(&index, &ChatSearchIndex::changed);
    index.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(index.count(), 0);
    QVERIFY(index.search(QString()).isEmpty());
    QVERIFY(index.search(QStringLiteral("alpha")).isEmpty());

    // The index remains usable after clearing.
    index.upsert(3, QStringLiteral("Gamma"), QString());
    QCOMPARE(index.count(), 1);
    const QVector<qint64> expected{3};
    QCOMPARE(index.search(QString()), expected);
}

void ChatSearchIndexTest::clearOnEmptyIsNoOp()
{
    ChatSearchIndex index;
    QSignalSpy spy(&index, &ChatSearchIndex::changed);

    index.clear();
    index.clear();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(index.count(), 0);
}

void ChatSearchIndexTest::searchDoesNotEmitChanged()
{
    ChatSearchIndex index;
    index.upsert(1, QStringLiteral("Alpha"), QStringLiteral("one"));
    index.upsert(2, QStringLiteral("Beta"), QStringLiteral("two"));

    QSignalSpy spy(&index, &ChatSearchIndex::changed);
    index.search(QStringLiteral("alpha"));
    index.search(QString());
    index.search(QString(), 100);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(index.count(), 2);
}

void ChatSearchIndexTest::changedOnlyOnRealChanges()
{
    ChatSearchIndex index;
    QSignalSpy spy(&index, &ChatSearchIndex::changed);

    index.upsert(1, QStringLiteral("Alpha"), QStringLiteral("one"));  // new row
    index.upsert(2, QStringLiteral("Beta"), QStringLiteral("two"));   // new row
    index.upsert(1, QStringLiteral("Alpha"), QStringLiteral("one"));  // moves to front
    index.upsert(1, QStringLiteral(" ALPHA "), QStringLiteral(" ONE ")); // already newest, same text
    index.upsert(0, QStringLiteral("ignored"), QString());            // invalid id
    index.remove(99);                                                  // absent
    index.remove(2);                                                   // present
    index.clear();                                                     // one row left
    index.clear();                                                     // already empty

    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(ChatSearchIndexTest)
#include "tst_chat_search_index.moc"
