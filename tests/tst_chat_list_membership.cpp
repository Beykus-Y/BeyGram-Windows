#include "telegram/ChatListMembership.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace beygram::telegram;

class ChatListMembershipTest final : public QObject {
    Q_OBJECT

private slots:
    void appliesMainPosition();
    void appliesArchivePosition();
    void appliesFolderPosition();
    void keepsSeveralListsIndependent();
    void readsPinnedFlag();
    void removesMembershipWithOrderZero();
    void preservesListsThePositionDoesNotName();
    void positionsArrayReplacesMembership();
    void acceptsStringEncodedOrder();
    void ignoresMissingAndMalformedPositions();
};

void ChatListMembershipTest::appliesMainPosition()
{
    const QJsonObject update = QJsonDocument::fromJson(R"({
        "@type": "updateChatPosition",
        "chat_id": 42,
        "position": {
            "list": { "@type": "chatListMain" },
            "order": 912345678901,
            "is_pinned": false
        }
    })")
                                   .object();

    const ChatListMembershipState state = membershipFromPositionUpdate(update);
    QVERIFY(state.inMain);
    QVERIFY(!state.inArchive);
    QVERIFY(state.folderPositions.isEmpty());

    // Applying a second position keeps the first list and adds the new one.
    const QJsonObject archive = QJsonDocument::fromJson(R"({
        "list": { "@type": "chatListArchive" },
        "order": 700,
        "is_pinned": false
    })")
                                    .object();
    const ChatListMembershipState both = applyPosition(state, archive);
    QVERIFY(both.inMain);
    QVERIFY(both.inArchive);
}

void ChatListMembershipTest::appliesArchivePosition()
{
    const QJsonObject update = QJsonDocument::fromJson(R"({
        "@type": "updateChatPosition",
        "chat_id": 42,
        "position": {
            "list": { "@type": "chatListArchive" },
            "order": 5,
            "is_pinned": true
        }
    })")
                                   .object();

    const ChatListMembershipState state = membershipFromPositionUpdate(update);
    QVERIFY(state.inArchive);
    QVERIFY(!state.inMain);
    QVERIFY(state.folderPositions.isEmpty());
}

void ChatListMembershipTest::appliesFolderPosition()
{
    const QJsonObject update = QJsonDocument::fromJson(R"({
        "@type": "updateChatPosition",
        "chat_id": 42,
        "position": {
            "list": { "@type": "chatListFolder", "chat_folder_id": 7 },
            "order": 500,
            "is_pinned": true
        }
    })")
                                   .object();

    const ChatListMembershipState state = membershipFromPositionUpdate(update);
    QVERIFY(!state.inMain);
    QVERIFY(!state.inArchive);
    QCOMPARE(state.folderPositions.size(), 1);
    QVERIFY(state.folderPositions.contains(7));
    QCOMPARE(state.folderPositions.value(7).order, qint64(500));
    QVERIFY(state.folderPositions.value(7).pinned);
}

void ChatListMembershipTest::keepsSeveralListsIndependent()
{
    const QJsonObject update = QJsonDocument::fromJson(R"({
        "@type": "updateChatPosition",
        "chat_id": 42,
        "positions": [
            {
                "list": { "@type": "chatListMain" },
                "order": 900,
                "is_pinned": false
            },
            {
                "list": { "@type": "chatListArchive" },
                "order": 800,
                "is_pinned": false
            },
            {
                "list": { "@type": "chatListFolder", "chat_folder_id": 3 },
                "order": 100,
                "is_pinned": false
            },
            {
                "list": { "@type": "chatListFolder", "chat_folder_id": 9 },
                "order": 200,
                "is_pinned": true
            }
        ]
    })")
                                   .object();

    const ChatListMembershipState state = membershipFromPositionUpdate(update);
    QVERIFY(state.inMain);
    QVERIFY(state.inArchive);
    QCOMPARE(state.folderPositions.size(), 2);
    QCOMPARE(state.folderPositions.value(3).order, qint64(100));
    QCOMPARE(state.folderPositions.value(9).order, qint64(200));
    QVERIFY(!state.folderPositions.value(3).pinned);
    QVERIFY(state.folderPositions.value(9).pinned);

    // Leaving one folder must not disturb the other lists.
    const QJsonObject left = QJsonDocument::fromJson(R"({
        "list": { "@type": "chatListFolder", "chat_folder_id": 9 },
        "order": 0,
        "is_pinned": false
    })")
                                 .object();
    const ChatListMembershipState after = applyPosition(state, left);
    QVERIFY(after.inMain);
    QVERIFY(after.inArchive);
    QCOMPARE(after.folderPositions.size(), 1);
    QVERIFY(after.folderPositions.contains(3));
    QVERIFY(!after.folderPositions.contains(9));
    QCOMPARE(after.folderPositions.value(3).order, qint64(100));
}

void ChatListMembershipTest::readsPinnedFlag()
{
    const QJsonObject pinned = QJsonDocument::fromJson(R"({
        "list": { "@type": "chatListMain" },
        "order": 1234,
        "is_pinned": true
    })")
                                   .object();
    ChatListMembershipState state;
    state = applyPosition(state, pinned);
    QVERIFY(state.inMain);

    // The position itself carries the flag; a later unpin keeps the order.
    const QJsonObject unpinned = QJsonDocument::fromJson(R"({
        "list": { "@type": "chatListFolder", "chat_folder_id": 2 },
        "order": 77,
        "is_pinned": false
    })")
                                     .object();
    state = applyPosition(state, unpinned);
    QVERIFY(!state.folderPositions.value(2).pinned);

    const QJsonObject repinned = QJsonDocument::fromJson(R"({
        "list": { "@type": "chatListFolder", "chat_folder_id": 2 },
        "order": 77,
        "is_pinned": true
    })")
                                     .object();
    state = applyPosition(state, repinned);
    QVERIFY(state.folderPositions.value(2).pinned);
    QCOMPARE(state.folderPositions.value(2).order, qint64(77));

    // A missing is_pinned reads as not pinned.
    const QJsonObject bare = QJsonDocument::fromJson(R"({
        "list": { "@type": "chatListFolder", "chat_folder_id": 2 },
        "order": 88
    })")
                                 .object();
    state = applyPosition(state, bare);
    QVERIFY(!state.folderPositions.value(2).pinned);
}

void ChatListMembershipTest::removesMembershipWithOrderZero()
{
    ChatListMembershipState state;
    state.inMain = true;
    state.inArchive = true;
    state.folderPositions.insert(4, ChatListPosition{400, true});

    const QJsonObject leaveMain = QJsonDocument::fromJson(R"({
        "list": { "@type": "chatListMain" },
        "order": 0,
        "is_pinned": false
    })")
                                      .object();
    const ChatListMembershipState noMain = applyPosition(state, leaveMain);
    QVERIFY(!noMain.inMain);
    QVERIFY(noMain.inArchive);
    QCOMPARE(noMain.folderPositions.size(), 1);

    const QJsonObject leaveArchive = QJsonDocument::fromJson(R"({
        "list": { "@type": "chatListArchive" },
        "order": 0,
        "is_pinned": false
    })")
                                         .object();
    const ChatListMembershipState noArchive = applyPosition(state, leaveArchive);
    QVERIFY(!noArchive.inArchive);
    QVERIFY(noArchive.inMain);

    const QJsonObject leaveFolder = QJsonDocument::fromJson(R"({
        "list": { "@type": "chatListFolder", "chat_folder_id": 4 },
        "order": 0,
        "is_pinned": false
    })")
                                        .object();
    const ChatListMembershipState noFolder = applyPosition(state, leaveFolder);
    QVERIFY(noFolder.inMain);
    QVERIFY(noFolder.inArchive);
    QVERIFY(noFolder.folderPositions.isEmpty());

    // Removing a list the chat is not in is harmless.
    const ChatListMembershipState empty;
    const ChatListMembershipState stillEmpty = applyPosition(empty, leaveFolder);
    QVERIFY(!stillEmpty.inMain);
    QVERIFY(!stillEmpty.inArchive);
    QVERIFY(stillEmpty.folderPositions.isEmpty());
}

void ChatListMembershipTest::preservesListsThePositionDoesNotName()
{
    ChatListMembershipState state;
    state.inMain = true;
    state.inArchive = true;
    state.folderPositions.insert(6, ChatListPosition{600, false});

    const QJsonObject noList = QJsonDocument::fromJson(R"({
        "order": 900,
        "is_pinned": true
    })")
                                   .object();
    const QJsonObject unknownList = QJsonDocument::fromJson(R"({
        "list": { "@type": "chatListStories" },
        "order": 900,
        "is_pinned": true
    })")
                                        .object();
    const QJsonObject numericList = QJsonDocument::fromJson(R"({
        "list": { "@type": 7 },
        "order": 900,
        "is_pinned": true
    })")
                                        .object();
    const QJsonObject folderWithoutId = QJsonDocument::fromJson(R"({
        "list": { "@type": "chatListFolder" },
        "order": 900,
        "is_pinned": true
    })")
                                            .object();

    const QJsonObject positions[] = {noList, unknownList, numericList, folderWithoutId};
    for (const QJsonObject& position : positions) {
        const ChatListMembershipState unchanged = applyPosition(state, position);
        QVERIFY(unchanged.inMain);
        QVERIFY(unchanged.inArchive);
        QCOMPARE(unchanged.folderPositions.size(), 1);
        QCOMPARE(unchanged.folderPositions.value(6).order, qint64(600));
    }
}

void ChatListMembershipTest::positionsArrayReplacesMembership()
{
    // The array is the chat's full membership, so lists it omits are not
    // joined even though another position would have set them.
    const QJsonObject update = QJsonDocument::fromJson(R"({
        "@type": "updateChatPosition",
        "chat_id": 42,
        "position": {
            "list": { "@type": "chatListArchive" },
            "order": 5,
            "is_pinned": false
        },
        "positions": [
            {
                "list": { "@type": "chatListMain" },
                "order": 300,
                "is_pinned": false
            },
            {
                "list": { "@type": "chatListFolder", "chat_folder_id": 2 },
                "order": 20,
                "is_pinned": true
            }
        ]
    })")
                                   .object();

    const ChatListMembershipState state = membershipFromPositionUpdate(update);
    QVERIFY(state.inMain);
    QVERIFY(!state.inArchive);
    QCOMPARE(state.folderPositions.size(), 1);
    QVERIFY(state.folderPositions.contains(2));
    QCOMPARE(state.folderPositions.value(2).order, qint64(20));
    QVERIFY(state.folderPositions.value(2).pinned);

    // An empty array means the chat is in no list at all.
    const QJsonObject emptyPositions = QJsonDocument::fromJson(R"({
        "@type": "updateChatPosition",
        "chat_id": 42,
        "positions": []
    })")
                                            .object();
    const ChatListMembershipState empty = membershipFromPositionUpdate(emptyPositions);
    QVERIFY(!empty.inMain);
    QVERIFY(!empty.inArchive);
    QVERIFY(empty.folderPositions.isEmpty());
}

void ChatListMembershipTest::acceptsStringEncodedOrder()
{
    // Transports that stringify int64 values must round-trip the order.
    const QJsonObject update = QJsonDocument::fromJson(R"({
        "@type": "updateChatPosition",
        "chat_id": "42",
        "positions": [
            {
                "list": { "@type": "chatListMain" },
                "order": "912345678901",
                "is_pinned": true
            },
            {
                "list": { "@type": "chatListFolder", "chat_folder_id": "8" },
                "order": "123456789012"
            }
        ]
    })")
                                   .object();

    const ChatListMembershipState state = membershipFromPositionUpdate(update);
    QVERIFY(state.inMain);
    QVERIFY(state.folderPositions.contains(8));
    QCOMPARE(state.folderPositions.value(8).order, qint64(123456789012LL));
    QVERIFY(!state.folderPositions.value(8).pinned);
}

void ChatListMembershipTest::ignoresMissingAndMalformedPositions()
{
    // Nothing usable at all.
    const ChatListMembershipState empty = membershipFromPositionUpdate(QJsonObject{});
    QVERIFY(!empty.inMain);
    QVERIFY(!empty.inArchive);
    QVERIFY(empty.folderPositions.isEmpty());

    const QJsonObject nullPosition = QJsonDocument::fromJson(R"({
        "@type": "updateChatPosition",
        "chat_id": 42,
        "position": null
    })")
                                         .object();
    QVERIFY(!membershipFromPositionUpdate(nullPosition).inMain);

    const QJsonObject nullList = QJsonDocument::fromJson(R"({
        "@type": "updateChatPosition",
        "chat_id": 42,
        "position": { "list": null, "order": 900, "is_pinned": true }
    })")
                                     .object();
    const ChatListMembershipState nullListState = membershipFromPositionUpdate(nullList);
    QVERIFY(!nullListState.inMain);
    QVERIFY(!nullListState.inArchive);
    QVERIFY(nullListState.folderPositions.isEmpty());

    // A non-array positions field falls back to the single position object.
    const QJsonObject wrongPositions = QJsonDocument::fromJson(R"({
        "@type": "updateChatPosition",
        "chat_id": 42,
        "positions": "not-a-list",
        "position": {
            "list": { "@type": "chatListMain" },
            "order": 11,
            "is_pinned": false
        }
    })")
                                            .object();
    QVERIFY(membershipFromPositionUpdate(wrongPositions).inMain);

    // Malformed entries are skipped, the valid neighbour survives.
    const QJsonObject mixed = QJsonDocument::fromJson(R"({
        "@type": "updateChatPosition",
        "chat_id": 42,
        "positions": [
            null,
            5,
            { "list": 7, "order": 900 },
            {
                "list": { "@type": "chatListArchive" },
                "order": 400,
                "is_pinned": true
            }
        ]
    })")
                                   .object();
    const ChatListMembershipState mixedState = membershipFromPositionUpdate(mixed);
    QVERIFY(!mixedState.inMain);
    QVERIFY(mixedState.inArchive);
    QVERIFY(mixedState.folderPositions.isEmpty());
}

QTEST_GUILESS_MAIN(ChatListMembershipTest)
#include "tst_chat_list_membership.moc"
