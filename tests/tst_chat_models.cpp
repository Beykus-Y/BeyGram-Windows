#include "telegram/ChatFilterModel.h"
#include "telegram/ChatListModel.h"
#include "telegram/MessageListModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class ChatModelsTest final : public QObject {
    Q_OBJECT

private slots:
    void chatModelExposesRoles();
    void chatModelMarksPhotoReady();
    void chatFilterMatchesTitleAndPreview();
    void messageModelSortsAndDeduplicates();
    void messageModelReplacesTemporaryId();
    void messageModelUpdatesAndRemoves();
};

void ChatModelsTest::chatModelExposesRoles()
{
    ChatData first;
    first.id = 1;
    first.title = QStringLiteral("Alpha");
    first.preview = QStringLiteral("Hello");
    first.unreadCount = 2;
    first.initials = QStringLiteral("A");
    first.accentIndex = 2;
    first.inMainList = true;
    first.order = 100;

    ChatData second;
    second.id = 2;
    second.title = QStringLiteral("Beta");
    second.preview = QStringLiteral("World");
    second.initials = QStringLiteral("B");
    second.inMainList = true;
    second.order = 50;

    ChatListModel model;
    model.setChats({second, first});
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.data(model.index(0), ChatListModel::ChatIdRole).toLongLong(), qint64(2));
    QCOMPARE(model.data(model.index(1), ChatListModel::TitleRole).toString(),
             QStringLiteral("Alpha"));
    QCOMPARE(model.data(model.index(1), ChatListModel::UnreadCountRole).toInt(), 2);
}

void ChatModelsTest::chatModelMarksPhotoReady()
{
    ChatData chat;
    chat.id = 1;
    chat.title = QStringLiteral("Alice");
    chat.photoFileId = 55;
    ChatListModel model;
    model.setChats({chat});
    QVERIFY(!model.data(model.index(0), ChatListModel::PhotoReadyRole).toBool());
    QCOMPARE(model.data(model.index(0), ChatListModel::PhotoRevisionRole).toInt(), 0);

    model.markPhotoReady(55);
    QVERIFY(model.data(model.index(0), ChatListModel::PhotoReadyRole).toBool());
    QCOMPARE(model.data(model.index(0), ChatListModel::PhotoRevisionRole).toInt(), 1);
}

void ChatModelsTest::chatFilterMatchesTitleAndPreview()
{
    ChatData first;
    first.id = 1;
    first.title = QStringLiteral("Alice");
    first.preview = QStringLiteral("Dinner tonight");
    ChatData second;
    second.id = 2;
    second.title = QStringLiteral("Bob");
    second.preview = QStringLiteral("Work report");

    ChatListModel source;
    ChatFilterModel filter;
    filter.setSourceModel(&source);
    source.setChats({first, second});
    QCOMPARE(filter.rowCount(), 2);

    filter.setFilterText(QStringLiteral("dinner"));
    QCOMPARE(filter.rowCount(), 1);
    QCOMPARE(filter.index(0, 0).data(ChatListModel::ChatIdRole).toLongLong(), qint64(1));

    filter.setFilterText(QStringLiteral("bob"));
    QCOMPARE(filter.rowCount(), 1);
    QCOMPARE(filter.index(0, 0).data(ChatListModel::ChatIdRole).toLongLong(), qint64(2));

    filter.setFilterText({});
    QCOMPARE(filter.rowCount(), 2);
}

void ChatModelsTest::messageModelSortsAndDeduplicates()
{
    MessageListModel model;
    MessageData later;
    later.id = 20;
    later.chatId = 1;
    later.text = QStringLiteral("later");
    MessageData earlier;
    earlier.id = 10;
    earlier.chatId = 1;
    earlier.text = QStringLiteral("earlier");

    model.addOrUpdate(later);
    model.addOrUpdate(earlier);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.data(model.index(0), MessageListModel::MessageIdRole).toLongLong(), qint64(10));
    QCOMPARE(model.data(model.index(1), MessageListModel::MessageIdRole).toLongLong(), qint64(20));

    later.text = QStringLiteral("replaced");
    model.addOrUpdate(later);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.data(model.index(1), MessageListModel::TextRole).toString(),
             QStringLiteral("replaced"));
}

void ChatModelsTest::messageModelReplacesTemporaryId()
{
    MessageListModel model;
    MessageData server;
    server.id = 100;
    server.chatId = 1;
    server.text = QStringLiteral("server id");
    model.addOrUpdate(server);

    MessageData temporary;
    temporary.id = 999999;
    temporary.chatId = 1;
    temporary.text = QStringLiteral("temporary");
    model.addOrUpdate(temporary);
    QCOMPARE(model.count(), 2);

    MessageData confirmed = server;
    confirmed.text = QStringLiteral("confirmed");
    model.replaceId(999999, confirmed);

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.data(model.index(0), MessageListModel::MessageIdRole).toLongLong(), qint64(100));
    QCOMPARE(model.data(model.index(0), MessageListModel::TextRole).toString(),
             QStringLiteral("confirmed"));
}

void ChatModelsTest::messageModelUpdatesAndRemoves()
{
    MessageListModel model;
    MessageData message;
    message.id = 5;
    message.chatId = 1;
    message.senderUserId = 77;
    message.senderName = QStringLiteral("Member");
    message.text = QStringLiteral("old");
    model.addOrUpdate(message);

    QVERIFY(model.updateSenderName(77, QStringLiteral("Alice")));
    QCOMPARE(model.data(model.index(0), MessageListModel::SenderNameRole).toString(),
             QStringLiteral("Alice"));
    QVERIFY(model.markEdited(5));
    QVERIFY(model.data(model.index(0), MessageListModel::EditedRole).toBool());

    QVERIFY(model.updateContent(
        5,
        QJsonObject{{QStringLiteral("@type"), QStringLiteral("messageText")},
                    {QStringLiteral("text"),
                     QJsonObject{{QStringLiteral("text"), QStringLiteral("new")}}}},
        true));
    QCOMPARE(model.data(model.index(0), MessageListModel::TextRole).toString(),
             QStringLiteral("new"));

    model.removeIds({5});
    QCOMPARE(model.count(), 0);
}

QTEST_GUILESS_MAIN(ChatModelsTest)
#include "tst_chat_models.moc"
