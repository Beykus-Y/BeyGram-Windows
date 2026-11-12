#include "telegram/ChatCache.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace beygram::telegram;

class ChatCacheTest final : public QObject {
    Q_OBJECT

private slots:
    void savesAndLoadsChats();
    void missingFileYieldsEmptySnapshot();
    void skipsMalformedRows();
    void rejectsWrongVersion();
    void emptyPathIsSafe();
};

namespace {

ChatData makeChat(qint64 id, const QString& title)
{
    ChatData chat;
    chat.id = id;
    chat.title = title;
    chat.type = QStringLiteral("chatTypePrivate");
    chat.preview = QStringLiteral("hello");
    chat.order = 900;
    chat.lastMessageDate = 1700000000;
    chat.lastMessageId = 42;
    chat.unreadCount = 3;
    chat.pinned = true;
    chat.muted = true;
    chat.inMainList = true;
    chat.archived = false;
    chat.archiveOrder = 0;
    chat.isChannel = false;
    chat.photoFileId = 77;
    chat.initials = QStringLiteral("A");
    chat.accentIndex = 2;
    return chat;
}

} // namespace

void ChatCacheTest::savesAndLoadsChats()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("chats.json"));

    const QVector<ChatData> chats{makeChat(1, QStringLiteral("Alice")),
                                  makeChat(2, QStringLiteral("Bob"))};
    QVERIFY(saveChatCache(path, chats));

    const ChatCacheSnapshot snapshot = loadChatCache(path);
    QCOMPARE(snapshot.chats.size(), 2);
    QCOMPARE(snapshot.chats.at(0).id, qint64(1));
    QCOMPARE(snapshot.chats.at(0).title, QStringLiteral("Alice"));
    QCOMPARE(snapshot.chats.at(0).unreadCount, 3);
    QVERIFY(snapshot.chats.at(0).pinned);
    QVERIFY(snapshot.chats.at(0).muted);
    QCOMPARE(snapshot.chats.at(0).photoFileId, 77);
    QCOMPARE(snapshot.chats.at(1).id, qint64(2));
    QVERIFY(snapshot.savedAt > 0);
}

void ChatCacheTest::missingFileYieldsEmptySnapshot()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const ChatCacheSnapshot snapshot =
        loadChatCache(QDir(dir.path()).filePath(QStringLiteral("absent.json")));
    QVERIFY(snapshot.chats.isEmpty());
    QCOMPARE(snapshot.savedAt, qint64(0));
}

void ChatCacheTest::skipsMalformedRows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("chats.json"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":1,"savedAt":5,"chats":[{"id":0},{"id":7,"title":"Ok"},"bad"]})");
    file.close();

    const ChatCacheSnapshot snapshot = loadChatCache(path);
    QCOMPARE(snapshot.chats.size(), 1);
    QCOMPARE(snapshot.chats.at(0).id, qint64(7));
    QCOMPARE(snapshot.chats.at(0).title, QStringLiteral("Ok"));
}

void ChatCacheTest::rejectsWrongVersion()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("chats.json"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":99,"chats":[{"id":7}]})");
    file.close();

    QVERIFY(loadChatCache(path).chats.isEmpty());
}

void ChatCacheTest::emptyPathIsSafe()
{
    QVERIFY(loadChatCache(QString()).chats.isEmpty());
    QVERIFY(!saveChatCache(QString(), {makeChat(1, QStringLiteral("A"))}));
}

QTEST_GUILESS_MAIN(ChatCacheTest)

#include "tst_chat_cache.moc"
