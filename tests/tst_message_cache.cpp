#include "telegram/MessageCache.h"

#include <QDir>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace beygram::telegram;

class MessageCacheTest final : public QObject {
    Q_OBJECT

private slots:
    void savesAndLoadsNewestPage();
    void capsStoredMessages();
    void missingChatYieldsEmpty();
    void removesFile();
    void invalidInputIsSafe();
};

namespace {

QJsonArray makeMessages(int count)
{
    QJsonArray array;
    for (int index = 0; index < count; ++index) {
        QJsonObject text;
        text.insert(QStringLiteral("text"), QStringLiteral("m%1").arg(index));

        QJsonObject content;
        content.insert(QStringLiteral("@type"), QStringLiteral("messageText"));
        content.insert(QStringLiteral("text"), text);

        QJsonObject message;
        message.insert(QStringLiteral("id"), 1000 + index);
        message.insert(QStringLiteral("chat_id"), 42);
        message.insert(QStringLiteral("date"), 1700000000 + index);
        message.insert(QStringLiteral("content"), content);
        array.append(message);
    }
    return array;
}

} // namespace

void MessageCacheTest::savesAndLoadsNewestPage()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QJsonArray messages = makeMessages(3);
    QVERIFY(saveCachedMessages(dir.path(), 42, messages));

    const CachedMessages cached = loadCachedMessages(dir.path(), 42);
    QCOMPARE(cached.messages.size(), 3);
    QCOMPARE(cached.messages.at(0).toObject().value(QStringLiteral("id")).toInt(), 1000);
    QVERIFY(cached.savedAt > 0);
}

void MessageCacheTest::capsStoredMessages()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QVERIFY(saveCachedMessages(dir.path(), 7, makeMessages(10), 4));
    const CachedMessages cached = loadCachedMessages(dir.path(), 7);
    QCOMPARE(cached.messages.size(), 4);
    QCOMPARE(cached.messages.at(3).toObject().value(QStringLiteral("id")).toInt(), 1003);
}

void MessageCacheTest::missingChatYieldsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(loadCachedMessages(dir.path(), 99).messages.isEmpty());
}

void MessageCacheTest::removesFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QVERIFY(saveCachedMessages(dir.path(), 5, makeMessages(2)));
    QVERIFY(!loadCachedMessages(dir.path(), 5).messages.isEmpty());

    removeCachedMessages(dir.path(), 5);
    QVERIFY(loadCachedMessages(dir.path(), 5).messages.isEmpty());
}

void MessageCacheTest::invalidInputIsSafe()
{
    QVERIFY(loadCachedMessages(QString(), 1).messages.isEmpty());
    QVERIFY(loadCachedMessages(QStringLiteral("C:/nonexistent-dir"), 1).messages.isEmpty());
    QVERIFY(!saveCachedMessages(QString(), 1, makeMessages(1)));
    QVERIFY(!saveCachedMessages(QStringLiteral("C:/nonexistent-dir"), 0, makeMessages(1)));
}

QTEST_GUILESS_MAIN(MessageCacheTest)

#include "tst_message_cache.moc"
