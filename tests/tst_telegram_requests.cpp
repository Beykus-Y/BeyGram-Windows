#include "telegram/TelegramRequests.h"

#include <QJsonArray>
#include <QtTest>

using namespace beygram::telegram;

class TelegramRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsSendTextRequest();
    void buildsEditRequest();
    void buildsHistoryRequest();
    void buildsViewAndDeleteRequests();
};

void TelegramRequestsTest::buildsSendTextRequest()
{
    const QJsonObject request = sendTextMessageRequest(42, QStringLiteral("Hello"));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(), QStringLiteral("sendMessage"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(request.value(QStringLiteral("reply_to")).isNull());

    const QJsonObject content =
        request.value(QStringLiteral("input_message_content")).toObject();
    QCOMPARE(content.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageText"));
    QCOMPARE(content.value(QStringLiteral("text"))
                 .toObject()
                 .value(QStringLiteral("text"))
                 .toString(),
             QStringLiteral("Hello"));
    QVERIFY(content.value(QStringLiteral("clear_draft")).toBool());
    QCOMPARE(content.value(QStringLiteral("text"))
                 .toObject()
                 .value(QStringLiteral("entities"))
                 .toArray()
                 .size(),
             0);
}

void TelegramRequestsTest::buildsEditRequest()
{
    const QJsonObject request =
        editTextMessageRequest(42, 900, QStringLiteral("Edited"));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("editMessageText"));
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(900));
    const QJsonObject content =
        request.value(QStringLiteral("input_message_content")).toObject();
    QCOMPARE(content.value(QStringLiteral("text"))
                 .toObject()
                 .value(QStringLiteral("text"))
                 .toString(),
             QStringLiteral("Edited"));
    QVERIFY(!content.value(QStringLiteral("clear_draft")).toBool());
}

void TelegramRequestsTest::buildsHistoryRequest()
{
    const QJsonObject request = getChatHistoryRequest(42, 1000, 50);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatHistory"));
    QCOMPARE(request.value(QStringLiteral("from_message_id")).toVariant().toLongLong(),
             qint64(1000));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);
    QVERIFY(!request.value(QStringLiteral("only_local")).toBool());
}

void TelegramRequestsTest::buildsViewAndDeleteRequests()
{
    const QJsonArray ids{100, 101};
    const QJsonObject view = viewMessagesRequest(42, ids, false);
    QCOMPARE(view.value(QStringLiteral("@type")).toString(), QStringLiteral("viewMessages"));
    QCOMPARE(view.value(QStringLiteral("message_ids")).toArray().size(), 2);
    QCOMPARE(view.value(QStringLiteral("source"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("messageSourceChatHistory"));
    QVERIFY(!view.value(QStringLiteral("force_read")).toBool());

    const QJsonObject remove = deleteMessagesRequest(42, ids, true);
    QCOMPARE(remove.value(QStringLiteral("@type")).toString(),
             QStringLiteral("deleteMessages"));
    QVERIFY(remove.value(QStringLiteral("revoke")).toBool());
}

QTEST_GUILESS_MAIN(TelegramRequestsTest)
#include "tst_telegram_requests.moc"
