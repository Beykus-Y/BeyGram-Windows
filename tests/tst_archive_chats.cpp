#include "telegram/ArchiveChats.h"

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

class ArchiveChatsTest final : public QObject {
    Q_OBJECT

private slots:
    void getArchivedChatsRequestUsesArchiveChatList();
    void loadArchivedChatsRequestUsesArchiveChatList();
    void listRequestsShareTheSameChatList();
    void clampsLimitToSupportedRange();
    void archivesChatThroughAddChatToList();
    void unarchivesChatThroughMainChatList();
    void archiveRequestSurvivesSerialization();
    void detectsChatsResponses();
};

void ArchiveChatsTest::getArchivedChatsRequestUsesArchiveChatList()
{
    const QJsonObject request = getArchivedChatsRequest(50);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(), QStringLiteral("getChats"));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);

    const QJsonValue chatList = request.value(QStringLiteral("chat_list"));
    QVERIFY(chatList.isObject());
    // The archive must never be sent as a null chat_list: null means Main.
    QVERIFY(!chatList.isNull());

    const QJsonObject chatListObject = chatList.toObject();
    QCOMPARE(chatListObject.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatListArchive"));
    QCOMPARE(chatListObject.size(), 1);
}

void ArchiveChatsTest::loadArchivedChatsRequestUsesArchiveChatList()
{
    const QJsonObject request = loadArchivedChatsRequest(30);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(), QStringLiteral("loadChats"));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 30);

    const QJsonObject chatList = request.value(QStringLiteral("chat_list")).toObject();
    QCOMPARE(chatList.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatListArchive"));
    QCOMPARE(chatList.size(), 1);
}

void ArchiveChatsTest::listRequestsShareTheSameChatList()
{
    QCOMPARE(QJsonDocument(getArchivedChatsRequest(10).value(QStringLiteral("chat_list")).toObject())
                 .toJson(QJsonDocument::Compact),
             QJsonDocument(loadArchivedChatsRequest(10).value(QStringLiteral("chat_list")).toObject())
                 .toJson(QJsonDocument::Compact));
}

void ArchiveChatsTest::clampsLimitToSupportedRange()
{
    QCOMPARE(getArchivedChatsRequest(0).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(getArchivedChatsRequest(-7).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(getArchivedChatsRequest(1).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(getArchivedChatsRequest(100).value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(getArchivedChatsRequest(101).value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(getArchivedChatsRequest(5000).value(QStringLiteral("limit")).toInt(), 100);

    QCOMPARE(loadArchivedChatsRequest(0).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(loadArchivedChatsRequest(-1).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(loadArchivedChatsRequest(999).value(QStringLiteral("limit")).toInt(), 100);
}

void ArchiveChatsTest::archivesChatThroughAddChatToList()
{
    const QJsonObject request = setChatArchivedRequest(42, true);

    // This schema has no toggleChatIsArchived; archiving is addChatToList with
    // the Archive chat list.
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("addChatToList"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));
    QVERIFY(!request.contains(QStringLiteral("is_archived")));

    const QJsonObject chatList = request.value(QStringLiteral("chat_list")).toObject();
    QCOMPARE(chatList.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatListArchive"));
    QCOMPARE(chatList.size(), 1);
}

void ArchiveChatsTest::unarchivesChatThroughMainChatList()
{
    const QJsonObject request = setChatArchivedRequest(42, false);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("addChatToList"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));

    const QJsonValue chatList = request.value(QStringLiteral("chat_list"));
    QVERIFY(chatList.isObject());
    QVERIFY(!chatList.isNull());
    QCOMPARE(chatList.toObject().value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatListMain"));

    // A large chat id must not be truncated.
    QCOMPARE(setChatArchivedRequest(4242424242LL, true)
                 .value(QStringLiteral("chat_id"))
                 .toVariant()
                 .toLongLong(),
             qint64(4242424242LL));
}

void ArchiveChatsTest::archiveRequestSurvivesSerialization()
{
    const QJsonObject archived = roundTrip(setChatArchivedRequest(7, true));
    QCOMPARE(archived.value(QStringLiteral("@type")).toString(),
             QStringLiteral("addChatToList"));
    QCOMPARE(archived.value(QStringLiteral("chat_list"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("chatListArchive"));

    const QJsonObject listed = roundTrip(getArchivedChatsRequest(25));
    QCOMPARE(listed.value(QStringLiteral("limit")).toInt(), 25);
    QCOMPARE(listed.value(QStringLiteral("chat_list"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("chatListArchive"));
}

void ArchiveChatsTest::detectsChatsResponses()
{
    QVERIFY(responseIsArchivedChats(parseObject(R"({"@type": "chats", "chat_ids": []})")));
    QVERIFY(responseIsArchivedChats(parseObject(R"({"@type": "chats"})")));
    QVERIFY(!responseIsArchivedChats(QJsonObject{}));
    QVERIFY(!responseIsArchivedChats(parseObject(R"({"@type": "ok"})")));
    QVERIFY(!responseIsArchivedChats(parseObject(R"({"@type": "error", "message": "404"})")));
    QVERIFY(!responseIsArchivedChats(parseObject(R"({"@type": "chat"})")));
}

QTEST_GUILESS_MAIN(ArchiveChatsTest)
#include "tst_archive_chats.moc"
