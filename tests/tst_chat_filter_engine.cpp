#include "telegram/ChatFilterEngine.h"

#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

ChatData makeChat(qint64 id, const QString& title, const QString& preview,
                  int unreadCount = 0, bool muted = false, bool pinned = false)
{
    ChatData chat;
    chat.id = id;
    chat.title = title;
    chat.preview = preview;
    chat.unreadCount = unreadCount;
    chat.muted = muted;
    chat.pinned = pinned;
    return chat;
}

QVector<ChatData> sampleChats()
{
    return {
        makeChat(1, QStringLiteral("Alice"), QStringLiteral("See you tomorrow"), 2, false, true),
        makeChat(2, QStringLiteral("Team Chat"), QStringLiteral("Build is green"), 4, true, false),
        makeChat(3, QStringLiteral("News"), QStringLiteral("Breaking story"), 0, false, false),
        makeChat(4, QStringLiteral("Bob"), QStringLiteral("Lunch at noon"), 0, true, true),
    };
}

QVector<qint64> idsOf(const QVector<ChatData>& chats)
{
    QVector<qint64> ids;
    ids.reserve(chats.size());
    for (const ChatData& chat : chats)
        ids.append(chat.id);
    return ids;
}

ChatFilterQuery textQuery(const QString& text)
{
    ChatFilterQuery query;
    query.text = text;
    return query;
}

} // namespace

class ChatFilterEngineTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithAnEmptyQuery();
    void setQueryEmitsOnlyOnRealChanges();
    void textMatchesTitleCaseInsensitively();
    void textMatchesPreviewCaseInsensitively();
    void textIgnoresOtherChatFields();
    void unreadOnlyKeepsChatsWithUnreadMessages();
    void mutedOnlyKeepsMutedChats();
    void pinnedOnlyKeepsPinnedChats();
    void flagsAreAnded();
    void folderFilterUsesMembership();
    void unknownFolderMatchesNothing();
    void folderFilterCombinesWithOtherCriteria();
    void setFolderMembersEmitsOnlyOnRealChanges();
    void setFolderMembersUpdatesTheVisibleResult();
    void applyPreservesInputOrder();
    void applyLeavesTheInputUntouched();
    void applyIsRepeatableAndUsesTheCurrentQuery();
    void matchCountAgreesWithApply();
    void emptyInputProducesEmptyResult();
    void clearResetsQueryAndMembers();
    void clearOnFreshEngineDoesNotEmit();
    void applyingDoesNotEmit();
};

void ChatFilterEngineTest::startsWithAnEmptyQuery()
{
    const ChatFilterEngine engine;
    const ChatFilterQuery query = engine.query();

    QVERIFY(query.text.isEmpty());
    QVERIFY(!query.unreadOnly);
    QVERIFY(!query.mutedOnly);
    QVERIFY(!query.pinnedOnly);
    QCOMPARE(query.folderId, qint64(0));

    // No criteria means every chat passes, in the order it was given.
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{1, 2, 3, 4}));
    QCOMPARE(engine.matchCount(sampleChats()), 4);
}

void ChatFilterEngineTest::setQueryEmitsOnlyOnRealChanges()
{
    ChatFilterEngine engine;
    QSignalSpy spy(&engine, &ChatFilterEngine::changed);

    // The default query is already in place, so it is not a change.
    engine.setQuery(ChatFilterQuery{});
    QCOMPARE(spy.count(), 0);

    ChatFilterQuery query;
    query.text = QStringLiteral("team");
    engine.setQuery(query);
    QCOMPARE(spy.count(), 1);

    engine.setQuery(query);
    QCOMPARE(spy.count(), 1);

    query.unreadOnly = true;
    engine.setQuery(query);
    QCOMPARE(spy.count(), 2);

    query.mutedOnly = true;
    engine.setQuery(query);
    QCOMPARE(spy.count(), 3);

    query.pinnedOnly = true;
    engine.setQuery(query);
    QCOMPARE(spy.count(), 4);

    query.folderId = 7;
    engine.setQuery(query);
    QCOMPARE(spy.count(), 5);

    // query() returns the full criteria that were last accepted.
    const ChatFilterQuery stored = engine.query();
    QCOMPARE(stored.text, QStringLiteral("team"));
    QVERIFY(stored.unreadOnly);
    QVERIFY(stored.mutedOnly);
    QVERIFY(stored.pinnedOnly);
    QCOMPARE(stored.folderId, qint64(7));
}

void ChatFilterEngineTest::textMatchesTitleCaseInsensitively()
{
    ChatFilterEngine engine;

    engine.setQuery(textQuery(QStringLiteral("alice")));
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{1}));

    engine.setQuery(textQuery(QStringLiteral("ALICE")));
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{1}));

    // Matching is a substring test, not a prefix or whole-word test.
    engine.setQuery(textQuery(QStringLiteral("lic")));
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{1}));

    engine.setQuery(textQuery(QStringLiteral("team chat")));
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{2}));

    engine.setQuery(textQuery(QStringLiteral("alicee")));
    QVERIFY(engine.apply(sampleChats()).isEmpty());
}

void ChatFilterEngineTest::textMatchesPreviewCaseInsensitively()
{
    ChatFilterEngine engine;

    engine.setQuery(textQuery(QStringLiteral("green")));
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{2}));

    engine.setQuery(textQuery(QStringLiteral("NOON")));
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{4}));

    engine.setQuery(textQuery(QStringLiteral("Breaking")));
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{3}));
}

void ChatFilterEngineTest::textIgnoresOtherChatFields()
{
    QVector<ChatData> chats{
        makeChat(1, QStringLiteral("Alice"), QStringLiteral("See you tomorrow"))};
    chats[0].type = QStringLiteral("secret");
    chats[0].initials = QStringLiteral("ZZ");
    chats[0].isChannel = true;

    ChatFilterEngine engine;
    engine.setQuery(textQuery(QStringLiteral("secret")));
    QVERIFY(engine.apply(chats).isEmpty());

    engine.setQuery(textQuery(QStringLiteral("zz")));
    QVERIFY(engine.apply(chats).isEmpty());

    // Only the title and the preview are searched.
    engine.setQuery(textQuery(QStringLiteral("alice")));
    QCOMPARE(idsOf(engine.apply(chats)), (QVector<qint64>{1}));
}

void ChatFilterEngineTest::unreadOnlyKeepsChatsWithUnreadMessages()
{
    ChatFilterEngine engine;
    ChatFilterQuery query;
    query.unreadOnly = true;
    engine.setQuery(query);

    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{1, 2}));
    QCOMPARE(engine.matchCount(sampleChats()), 2);
}

void ChatFilterEngineTest::mutedOnlyKeepsMutedChats()
{
    ChatFilterEngine engine;
    ChatFilterQuery query;
    query.mutedOnly = true;
    engine.setQuery(query);

    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{2, 4}));
    QCOMPARE(engine.matchCount(sampleChats()), 2);
}

void ChatFilterEngineTest::pinnedOnlyKeepsPinnedChats()
{
    ChatFilterEngine engine;
    ChatFilterQuery query;
    query.pinnedOnly = true;
    engine.setQuery(query);

    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{1, 4}));
    QCOMPARE(engine.matchCount(sampleChats()), 2);
}

void ChatFilterEngineTest::flagsAreAnded()
{
    ChatFilterEngine engine;

    ChatFilterQuery query;
    query.unreadOnly = true;
    query.mutedOnly = true;
    engine.setQuery(query);
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{2}));

    query.pinnedOnly = true;
    engine.setQuery(query);
    QVERIFY(engine.apply(sampleChats()).isEmpty());

    // A text criterion is one more condition, not an alternative.
    query.pinnedOnly = false;
    query.text = QStringLiteral("alice");
    engine.setQuery(query);
    QVERIFY(engine.apply(sampleChats()).isEmpty());
}

void ChatFilterEngineTest::folderFilterUsesMembership()
{
    ChatFilterEngine engine;
    engine.setFolderMembers(7, {3, 1});

    ChatFilterQuery query;
    query.folderId = 7;
    engine.setQuery(query);

    // The input order wins over the membership order.
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{1, 3}));
    QCOMPARE(engine.matchCount(sampleChats()), 2);
}

void ChatFilterEngineTest::unknownFolderMatchesNothing()
{
    ChatFilterEngine engine;

    ChatFilterQuery query;
    query.folderId = 42;
    engine.setQuery(query);

    QVERIFY(engine.apply(sampleChats()).isEmpty());
    QCOMPARE(engine.matchCount(sampleChats()), 0);

    // Registering a different folder does not make 42 known.
    engine.setFolderMembers(7, {1, 2});
    QVERIFY(engine.apply(sampleChats()).isEmpty());

    // An explicitly emptied folder is unknown as well.
    engine.setFolderMembers(42, {1, 2});
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{1, 2}));
    engine.setFolderMembers(42, {});
    QVERIFY(engine.apply(sampleChats()).isEmpty());
}

void ChatFilterEngineTest::folderFilterCombinesWithOtherCriteria()
{
    ChatFilterEngine engine;
    engine.setFolderMembers(7, {1, 2, 3});

    ChatFilterQuery query;
    query.folderId = 7;
    engine.setQuery(query);
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{1, 2, 3}));

    query.unreadOnly = true;
    engine.setQuery(query);
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{1, 2}));

    query.text = QStringLiteral("team");
    engine.setQuery(query);
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{2}));

    query.mutedOnly = true;
    engine.setQuery(query);
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{2}));

    query.pinnedOnly = true;
    engine.setQuery(query);
    QVERIFY(engine.apply(sampleChats()).isEmpty());
}

void ChatFilterEngineTest::setFolderMembersEmitsOnlyOnRealChanges()
{
    ChatFilterEngine engine;
    QSignalSpy spy(&engine, &ChatFilterEngine::changed);

    // Emptying a folder that is not known changes nothing.
    engine.setFolderMembers(7, {});
    QCOMPARE(spy.count(), 0);

    engine.setFolderMembers(7, {1, 2});
    QCOMPARE(spy.count(), 1);

    engine.setFolderMembers(7, {1, 2});
    QCOMPARE(spy.count(), 1);

    // Duplicates are not distinct members, so this is still the same folder.
    engine.setFolderMembers(7, {1, 1, 2, 2});
    QCOMPARE(spy.count(), 1);

    engine.setFolderMembers(7, {2});
    QCOMPARE(spy.count(), 2);

    engine.setFolderMembers(7, {});
    QCOMPARE(spy.count(), 3);

    engine.setFolderMembers(7, {});
    QCOMPARE(spy.count(), 3);

    // Another folder is independent state.
    engine.setFolderMembers(8, {1});
    QCOMPARE(spy.count(), 4);
}

void ChatFilterEngineTest::setFolderMembersUpdatesTheVisibleResult()
{
    ChatFilterEngine engine;

    ChatFilterQuery query;
    query.folderId = 7;
    engine.setQuery(query);

    engine.setFolderMembers(7, {2});
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{2}));

    engine.setFolderMembers(7, {4, 2});
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{2, 4}));

    engine.setFolderMembers(7, {});
    QVERIFY(engine.apply(sampleChats()).isEmpty());
}

void ChatFilterEngineTest::applyPreservesInputOrder()
{
    ChatFilterEngine engine;

    const QVector<ChatData> chats{
        makeChat(4, QStringLiteral("Bob"), QStringLiteral("Lunch at noon"), 0, true, true),
        makeChat(3, QStringLiteral("News"), QStringLiteral("Breaking story")),
        makeChat(2, QStringLiteral("Team Chat"), QStringLiteral("Build is green"), 4, true),
        makeChat(1, QStringLiteral("Alice"), QStringLiteral("See you tomorrow"), 2, false, true),
    };

    ChatFilterQuery query;
    query.unreadOnly = true;
    engine.setQuery(query);
    QCOMPARE(idsOf(engine.apply(chats)), (QVector<qint64>{2, 1}));

    query.unreadOnly = false;
    query.pinnedOnly = true;
    engine.setQuery(query);
    QCOMPARE(idsOf(engine.apply(chats)), (QVector<qint64>{4, 1}));
}

void ChatFilterEngineTest::applyLeavesTheInputUntouched()
{
    ChatFilterEngine engine;
    engine.setQuery(textQuery(QStringLiteral("team")));

    QVector<ChatData> input = sampleChats();
    QVector<ChatData> result = engine.apply(input);

    QCOMPARE(idsOf(input), (QVector<qint64>{1, 2, 3, 4}));
    QCOMPARE(idsOf(result), (QVector<qint64>{2}));

    // The result is a copy: editing it cannot reach the caller's chats.
    QCOMPARE(result.size(), qsizetype(1));
    result[0].title = QStringLiteral("changed");
    QCOMPARE(input.at(1).title, QStringLiteral("Team Chat"));
}

void ChatFilterEngineTest::applyIsRepeatableAndUsesTheCurrentQuery()
{
    ChatFilterEngine engine;

    engine.setQuery(textQuery(QStringLiteral("team")));
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{2}));
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{2}));

    engine.setQuery(textQuery(QStringLiteral("story")));
    QCOMPARE(idsOf(engine.apply(sampleChats())), (QVector<qint64>{3}));
}

void ChatFilterEngineTest::matchCountAgreesWithApply()
{
    ChatFilterEngine engine;

    QCOMPARE(engine.matchCount(sampleChats()), 4);

    ChatFilterQuery query;
    query.unreadOnly = true;
    engine.setQuery(query);
    QCOMPARE(engine.matchCount(sampleChats()), 2);
    QCOMPARE(engine.matchCount(sampleChats()), int(engine.apply(sampleChats()).size()));

    query.mutedOnly = true;
    engine.setQuery(query);
    QCOMPARE(engine.matchCount(sampleChats()), 1);
    QCOMPARE(engine.matchCount(sampleChats()), int(engine.apply(sampleChats()).size()));

    query.folderId = 99;
    engine.setQuery(query);
    QCOMPARE(engine.matchCount(sampleChats()), 0);
    QCOMPARE(engine.matchCount(sampleChats()), int(engine.apply(sampleChats()).size()));
}

void ChatFilterEngineTest::emptyInputProducesEmptyResult()
{
    ChatFilterEngine engine;

    QVERIFY(engine.apply({}).isEmpty());
    QCOMPARE(engine.matchCount({}), 0);

    ChatFilterQuery query;
    query.text = QStringLiteral("anything");
    query.unreadOnly = true;
    query.mutedOnly = true;
    query.pinnedOnly = true;
    query.folderId = 7;
    engine.setQuery(query);
    engine.setFolderMembers(7, {1, 2});

    QVERIFY(engine.apply({}).isEmpty());
    QCOMPARE(engine.matchCount({}), 0);
}

void ChatFilterEngineTest::clearResetsQueryAndMembers()
{
    ChatFilterEngine engine;

    ChatFilterQuery query;
    query.text = QStringLiteral("team");
    query.unreadOnly = true;
    query.folderId = 7;
    engine.setQuery(query);
    engine.setFolderMembers(7, {1, 2});

    QSignalSpy spy(&engine, &ChatFilterEngine::changed);

    engine.clear();
    QCOMPARE(spy.count(), 1);

    const ChatFilterQuery cleared = engine.query();
    QVERIFY(cleared.text.isEmpty());
    QVERIFY(!cleared.unreadOnly);
    QVERIFY(!cleared.mutedOnly);
    QVERIFY(!cleared.pinnedOnly);
    QCOMPARE(cleared.folderId, qint64(0));
    QCOMPARE(engine.apply(sampleChats()).size(), qsizetype(4));

    // The memberships were dropped with the query, so folder 7 is unknown now.
    ChatFilterQuery folderOnly;
    folderOnly.folderId = 7;
    engine.setQuery(folderOnly);
    QVERIFY(engine.apply(sampleChats()).isEmpty());

    engine.clear();
    QCOMPARE(spy.count(), 3);

    // Already empty: no second notification for the same state.
    engine.clear();
    QCOMPARE(spy.count(), 3);
}

void ChatFilterEngineTest::clearOnFreshEngineDoesNotEmit()
{
    ChatFilterEngine engine;
    QSignalSpy spy(&engine, &ChatFilterEngine::changed);

    engine.clear();
    QCOMPARE(spy.count(), 0);

    // Nothing was reset, so the default query is still in effect.
    QCOMPARE(engine.apply(sampleChats()).size(), qsizetype(4));
}

void ChatFilterEngineTest::applyingDoesNotEmit()
{
    ChatFilterEngine engine;
    engine.setQuery(textQuery(QStringLiteral("team")));

    QSignalSpy spy(&engine, &ChatFilterEngine::changed);

    engine.apply(sampleChats());
    engine.apply({});
    engine.matchCount(sampleChats());
    engine.matchCount({});
    engine.query();

    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(ChatFilterEngineTest)
#include "tst_chat_filter_engine.moc"
