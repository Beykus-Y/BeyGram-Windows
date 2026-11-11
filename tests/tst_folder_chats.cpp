#include "telegram/FolderChats.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

// Requests are compared after a JSON round trip so the assertions see exactly
// what would be handed to TDLib.
QJsonObject roundTrip(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

} // namespace

class FolderChatsTest final : public QObject {
    Q_OBJECT

private slots:
    void mainListGetChatsRequestUsesNullChatList();
    void folderGetChatsRequestUsesChatListFolder();
    void clampsLimitToSupportedRange();
    void loadChatsRequestShapes();
    void parsesChatIdsFromMixedArray();
    void parsesNumericStringIds();
    void skipsDuplicatesPreservingOrder();
    void returnsEmptyForMissingChatIds();
    void detectsChatsResponses();
};

void FolderChatsTest::mainListGetChatsRequestUsesNullChatList()
{
    const QJsonObject request = getChatsForFolderRequest(0, 50);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(), QStringLiteral("getChats"));
    QVERIFY(request.contains(QStringLiteral("chat_list")));
    QVERIFY(request.value(QStringLiteral("chat_list")).isNull());
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);

    // The null chat_list must survive serialization as an explicit null.
    const QJsonObject serialized = roundTrip(request);
    QVERIFY(serialized.value(QStringLiteral("chat_list")).isNull());
    QCOMPARE(serialized.value(QStringLiteral("@type")).toString(), QStringLiteral("getChats"));
}

void FolderChatsTest::folderGetChatsRequestUsesChatListFolder()
{
    const QJsonObject request = getChatsForFolderRequest(42, 20);
    const QJsonValue chatList = request.value(QStringLiteral("chat_list"));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(), QStringLiteral("getChats"));
    QVERIFY(chatList.isObject());
    const QJsonObject chatListObject = chatList.toObject();
    QCOMPARE(chatListObject.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatListFolder"));
    QCOMPARE(chatListObject.value(QStringLiteral("chat_folder_id")).toVariant().toLongLong(),
             qint64(42));

    // A large folder id must not be truncated.
    const QJsonObject bigRequest = getChatsForFolderRequest(4242424242LL, 20);
    QCOMPARE(bigRequest.value(QStringLiteral("chat_list"))
                 .toObject()
                 .value(QStringLiteral("chat_folder_id"))
                 .toVariant()
                 .toLongLong(),
             qint64(4242424242LL));
}

void FolderChatsTest::clampsLimitToSupportedRange()
{
    QCOMPARE(getChatsForFolderRequest(0, 0).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(getChatsForFolderRequest(0, -7).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(getChatsForFolderRequest(0, 1).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(getChatsForFolderRequest(0, 100).value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(getChatsForFolderRequest(0, 101).value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(getChatsForFolderRequest(0, 5000).value(QStringLiteral("limit")).toInt(), 100);

    QCOMPARE(loadChatsForFolderRequest(0, 0).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(loadChatsForFolderRequest(0, 999).value(QStringLiteral("limit")).toInt(), 100);
}

void FolderChatsTest::loadChatsRequestShapes()
{
    const QJsonObject mainRequest = loadChatsForFolderRequest(0, 30);
    QCOMPARE(mainRequest.value(QStringLiteral("@type")).toString(),
             QStringLiteral("loadChats"));
    QVERIFY(mainRequest.value(QStringLiteral("chat_list")).isNull());
    QCOMPARE(mainRequest.value(QStringLiteral("limit")).toInt(), 30);

    const QJsonObject folderRequest = loadChatsForFolderRequest(7, 15);
    QCOMPARE(folderRequest.value(QStringLiteral("@type")).toString(),
             QStringLiteral("loadChats"));
    const QJsonObject chatList = folderRequest.value(QStringLiteral("chat_list")).toObject();
    QCOMPARE(chatList.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatListFolder"));
    QCOMPARE(chatList.value(QStringLiteral("chat_folder_id")).toVariant().toLongLong(),
             qint64(7));
    QCOMPARE(folderRequest.value(QStringLiteral("limit")).toInt(), 15);

    // Both requests share the same chat_list encoding for the same folder.
    QCOMPARE(QJsonDocument(getChatsForFolderRequest(7, 15).value(QStringLiteral("chat_list"))
                               .toObject())
                 .toJson(QJsonDocument::Compact),
             QJsonDocument(loadChatsForFolderRequest(7, 15).value(QStringLiteral("chat_list"))
                               .toObject())
                 .toJson(QJsonDocument::Compact));
}

void FolderChatsTest::parsesChatIdsFromMixedArray()
{
    const QJsonObject response = parseObject(R"({
        "@type": "chats",
        "total_count": 6,
        "chat_ids": [111, "222", 0, "abc", 333, null, {"id": 444}, true]
    })");

    const QVector<qint64> ids = chatIdsFromResponse(response);
    QCOMPARE(ids, (QVector<qint64>{111, 222, 333}));
}

void FolderChatsTest::parsesNumericStringIds()
{
    const QJsonObject response = parseObject(R"({
        "@type": "chats",
        "chat_ids": ["-1001234567890", "4242424242", " 77 ", "0", ""]
    })");

    const QVector<qint64> ids = chatIdsFromResponse(response);
    QCOMPARE(ids.size(), 3);
    QCOMPARE(ids.at(0), qint64(-1001234567890LL));
    QCOMPARE(ids.at(1), qint64(4242424242LL));
    QCOMPARE(ids.at(2), qint64(77));
}

void FolderChatsTest::skipsDuplicatesPreservingOrder()
{
    const QJsonObject response = parseObject(R"({
        "@type": "chats",
        "chat_ids": [5, "5", 9, 5, "9", 12, 12]
    })");

    const QVector<qint64> ids = chatIdsFromResponse(response);
    QCOMPARE(ids, (QVector<qint64>{5, 9, 12}));
}

void FolderChatsTest::returnsEmptyForMissingChatIds()
{
    QVERIFY(chatIdsFromResponse(QJsonObject{}).isEmpty());
    QVERIFY(chatIdsFromResponse(parseObject(R"({"@type": "chats"})")).isEmpty());
    QVERIFY(chatIdsFromResponse(parseObject(R"({"@type": "chats", "chat_ids": []})")).isEmpty());
    QVERIFY(chatIdsFromResponse(parseObject(R"({"@type": "chats", "chat_ids": 42})")).isEmpty());
    QVERIFY(chatIdsFromResponse(parseObject(R"({"@type": "chats", "chat_ids": [0, "0", null]})"))
                .isEmpty());
}

void FolderChatsTest::detectsChatsResponses()
{
    QVERIFY(responseIsChats(parseObject(R"({"@type": "chats", "chat_ids": []})")));
    QVERIFY(responseIsChats(parseObject(R"({"@type": "chats"})")));
    QVERIFY(!responseIsChats(QJsonObject{}));
    QVERIFY(!responseIsChats(parseObject(R"({"@type": "ok"})")));
    QVERIFY(!responseIsChats(parseObject(R"({"@type": "error", "message": "404"})")));
    QVERIFY(!responseIsChats(parseObject(R"({"@type": "chat"})")));
}

QTEST_GUILESS_MAIN(FolderChatsTest)
#include "tst_folder_chats.moc"
