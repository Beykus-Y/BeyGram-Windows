#include "telegram/ChatFolders.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

} // namespace

class ChatFoldersTest final : public QObject {
    Q_OBJECT

private slots:
    void mainFolderIsFirstAndFoldersFollowPositions();
    void parsesFormattedAndPlainTitles();
    void parsesArchiveFlagAndArchiveChatList();
    void ignoresUnknownUpdate();
    void synthesizesMainFolder();
    void buildsRequestShapes();
    void parsesNumericStringIds();
};

void ChatFoldersTest::mainFolderIsFirstAndFoldersFollowPositions()
{
    const QJsonObject update = parseObject(R"({
        "@type": "updateChatFolders",
        "chat_folders": [
            {
                "@type": "chatFolderInfo",
                "id": 2,
                "title": {"@type": "chatFolderName",
                          "text": {"text": "Work"},
                          "animate_custom_emoji": false},
                "position": 2
            },
            {
                "@type": "chatFolderInfo",
                "id": 0,
                "title": {"@type": "chatFolderName",
                          "text": {"text": "All chats"},
                          "animate_custom_emoji": false},
                "position": 0,
                "is_main": true
            },
            {
                "@type": "chatFolderInfo",
                "id": 1,
                "title": {"@type": "chatFolderName",
                          "text": {"text": "Friends"},
                          "animate_custom_emoji": false},
                "position": 1
            }
        ]
    })");

    const ChatFoldersState state = chatFoldersFromUpdate(update);
    QCOMPARE(state.folders.size(), 3);
    QCOMPARE(state.folders.at(0).id, qint64(0));
    QVERIFY(state.folders.at(0).isMain);
    QCOMPARE(state.folders.at(0).title, QStringLiteral("All chats"));
    QCOMPARE(state.folders.at(1).id, qint64(1));
    QCOMPARE(state.folders.at(1).title, QStringLiteral("Friends"));
    QVERIFY(!state.folders.at(1).isMain);
    QCOMPARE(state.folders.at(2).id, qint64(2));
    QCOMPARE(state.folders.at(2).title, QStringLiteral("Work"));
}

void ChatFoldersTest::parsesFormattedAndPlainTitles()
{
    const QJsonObject update = parseObject(R"({
        "@type": "updateChatFolders",
        "chat_folders": [
            {
                "@type": "chatFolderInfo",
                "id": 3,
                "title": {"@type": "chatFolderName",
                          "text": {"text": "Formatted"},
                          "animate_custom_emoji": true},
                "position": 1
            },
            {
                "@type": "chatFolderInfo",
                "id": 4,
                "title": "Plain string",
                "position": 2
            },
            {
                "@type": "chatFolderInfo",
                "id": 5,
                "name": {"@type": "chatFolderName",
                         "text": {"text": "Named"},
                         "animate_custom_emoji": false},
                "position": 3
            },
            {
                "@type": "chatFolderInfo",
                "id": 6,
                "title": {"@type": "formattedText",
                          "text": "Bare formatted text",
                          "entities": []},
                "position": 4
            }
        ]
    })");

    const ChatFoldersState state = chatFoldersFromUpdate(update);
    QCOMPARE(state.folders.size(), 5);
    QCOMPARE(state.folders.at(1).id, qint64(3));
    QCOMPARE(state.folders.at(1).title, QStringLiteral("Formatted"));
    QCOMPARE(state.folders.at(2).id, qint64(4));
    QCOMPARE(state.folders.at(2).title, QStringLiteral("Plain string"));
    QCOMPARE(state.folders.at(3).id, qint64(5));
    QCOMPARE(state.folders.at(3).title, QStringLiteral("Named"));
    QCOMPARE(state.folders.at(4).id, qint64(6));
    QCOMPARE(state.folders.at(4).title, QStringLiteral("Bare formatted text"));
}

void ChatFoldersTest::parsesArchiveFlagAndArchiveChatList()
{
    const QJsonObject withArchive = parseObject(R"({
        "@type": "updateChatFolders",
        "has_archive": true,
        "archive_chat_list": {"@type": "chatListArchive"},
        "chat_folders": [
            {
                "@type": "chatFolderInfo",
                "id": 0,
                "title": "All chats",
                "position": 0,
                "is_main": true
            }
        ]
    })");

    const ChatFoldersState archiveState = chatFoldersFromUpdate(withArchive);
    QVERIFY(archiveState.hasArchive);
    QCOMPARE(archiveState.archiveChatListId, qint64(0));
    QVERIFY(archiveState.folders.at(0).hasArchive);

    const QJsonObject withoutArchive = parseObject(R"({
        "@type": "updateChatFolders",
        "chat_folders": [
            {
                "@type": "chatFolderInfo",
                "id": 7,
                "title": "No archive",
                "position": 1
            }
        ]
    })");

    const ChatFoldersState plainState = chatFoldersFromUpdate(withoutArchive);
    QVERIFY(!plainState.hasArchive);
    QCOMPARE(plainState.archiveChatListId, qint64(0));
    QVERIFY(!plainState.folders.at(0).hasArchive);
}

void ChatFoldersTest::ignoresUnknownUpdate()
{
    const QJsonObject update = parseObject(R"({
        "@type": "updateNewMessage",
        "chat_folders": [
            {"@type": "chatFolderInfo", "id": 1, "title": "Ignored", "position": 1}
        ],
        "has_archive": true
    })");

    const ChatFoldersState state = chatFoldersFromUpdate(update);
    QVERIFY(state.folders.isEmpty());
    QVERIFY(!state.hasArchive);
    QCOMPARE(state.archiveChatListId, qint64(0));
}

void ChatFoldersTest::synthesizesMainFolder()
{
    const QJsonObject update = parseObject(R"({
        "@type": "updateChatFolders",
        "main_chat_list_position": 3,
        "chat_folders": [
            {
                "@type": "chatFolderInfo",
                "id": 9,
                "title": "Only custom",
                "position": 1
            }
        ]
    })");

    const ChatFoldersState state = chatFoldersFromUpdate(update);
    QCOMPARE(state.folders.size(), 2);
    const ChatFolderInfo& main = state.folders.at(0);
    QCOMPARE(main.id, qint64(0));
    QCOMPARE(main.title, QStringLiteral("All chats"));
    QVERIFY(main.isMain);
    QCOMPARE(main.position, 3);
    QCOMPARE(state.folders.at(1).id, qint64(9));
}

void ChatFoldersTest::buildsRequestShapes()
{
    const QJsonObject foldersRequest = getChatFoldersRequest();
    QCOMPARE(foldersRequest.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatFolders"));
    QVERIFY(foldersRequest.contains(QStringLiteral("chat_list")));
    QVERIFY(foldersRequest.value(QStringLiteral("chat_list")).isNull());
    QCOMPARE(foldersRequest.value(QStringLiteral("limit")).toInt(), 100);

    const QJsonObject folderRequest = getChatFolderRequest(42);
    QCOMPARE(folderRequest.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatFolder"));
    QCOMPARE(folderRequest.value(QStringLiteral("chat_folder_id")).toVariant().toLongLong(),
             qint64(42));

    const QJsonObject mainRequest = getChatFolderRequest(0);
    QCOMPARE(mainRequest.value(QStringLiteral("chat_folder_id")).toVariant().toLongLong(),
             qint64(0));
}

void ChatFoldersTest::parsesNumericStringIds()
{
    const QJsonObject update = parseObject(R"({
        "@type": "updateChatFolders",
        "chat_folders": [
            {
                "@type": "chatFolderInfo",
                "id": "0",
                "title": "All chats",
                "position": "0",
                "is_main": true
            },
            {
                "@type": "chatFolderInfo",
                "id": "4242424242",
                "title": {"@type": "chatFolderName",
                          "text": {"text": "Big id"},
                          "animate_custom_emoji": false},
                "icon": {"@type": "chatFolderIcon", "custom_emoji_id": "5368324170671202286"},
                "position": "1",
                "unread_count": "12"
            }
        ]
    })");

    const ChatFoldersState state = chatFoldersFromUpdate(update);
    QCOMPARE(state.folders.size(), 2);
    QCOMPARE(state.folders.at(0).id, qint64(0));
    QVERIFY(state.folders.at(0).isMain);
    QCOMPARE(state.folders.at(1).id, qint64(4242424242LL));
    QCOMPARE(state.folders.at(1).position, 1);
    QCOMPARE(state.folders.at(1).icon, QStringLiteral("5368324170671202286"));
    QCOMPARE(state.folders.at(1).unreadCount, 12);
}

QTEST_GUILESS_MAIN(ChatFoldersTest)
#include "tst_chat_folders.moc"
