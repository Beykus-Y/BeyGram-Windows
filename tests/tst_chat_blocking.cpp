#include "telegram/ChatBlocking.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
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

// reportOption.id is bytes; the JSON interface carries it base64-encoded.
QString base64Id(const char* text)
{
    return QString::fromLatin1(QByteArray(text).toBase64());
}

QJsonObject reportOption(const QString& id, const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("reportOption")},
        {QStringLiteral("id"), id},
        {QStringLiteral("text"), text},
    };
}

} // namespace

class ChatBlockingTest final : public QObject {
    Q_OBJECT

private slots:
    void senderVariantsMatchSchema();
    void blocksUserSender();
    void unblocksUserSenderWithNullBlockList();
    void blocksChatSender();
    void reportMessageStartsWithEmptyOptionId();
    void reportMessageKeepsFreeFormReasonAsText();
    void mapsReasonNames();
    void listsReasonTypes();
    void picksReportOptionForReason();
    void reportOptionRequestCarriesChosenOption();
    void clampsBlockedSendersPaging();
    void blockedSendersRequestShape();
    void detectsMessageSendersResponse();
    void blocksSenderFromReplies();
    void requestsSurviveSerialization();
};

void ChatBlockingTest::senderVariantsMatchSchema()
{
    const QJsonObject user = messageSenderUser(42);
    QCOMPARE(user.size(), 2);
    QCOMPARE(user.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(user.value(QStringLiteral("user_id")).toVariant().toLongLong(), qint64(42));
    // The variant must not leak the other identifier.
    QVERIFY(!user.contains(QStringLiteral("chat_id")));

    const QJsonObject chat = messageSenderChat(-1001234567890LL);
    QCOMPARE(chat.size(), 2);
    QCOMPARE(chat.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderChat"));
    QCOMPARE(chat.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(-1001234567890LL));
    QVERIFY(!chat.contains(QStringLiteral("user_id")));

    // int53 identifiers must not be truncated.
    QCOMPARE(messageSenderUser(9007199254740991LL)
                 .value(QStringLiteral("user_id"))
                 .toVariant()
                 .toLongLong(),
             qint64(9007199254740991LL));
}

void ChatBlockingTest::blocksUserSender()
{
    const QJsonObject request = blockSenderRequest(42, true);

    // This schema has no toggleMessageSenderIsBlocked.
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setMessageSenderBlockList"));
    QVERIFY(!request.contains(QStringLiteral("is_blocked")));
    QVERIFY(!request.contains(QStringLiteral("user_id")));

    const QJsonObject sender = request.value(QStringLiteral("sender_id")).toObject();
    QCOMPARE(sender.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(sender.value(QStringLiteral("user_id")).toVariant().toLongLong(), qint64(42));

    const QJsonValue blockList = request.value(QStringLiteral("block_list"));
    QVERIFY(blockList.isObject());
    QCOMPARE(blockList.toObject().value(QStringLiteral("@type")).toString(),
             QStringLiteral("blockListMain"));
    QCOMPARE(blockList.toObject().size(), 1);
}

void ChatBlockingTest::unblocksUserSenderWithNullBlockList()
{
    const QJsonObject request = blockSenderRequest(42, false);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setMessageSenderBlockList"));
    // BlockList has no "none" variant, so unblocking is the null block_list.
    const QJsonValue blockList = request.value(QStringLiteral("block_list"));
    QVERIFY(blockList.isNull());
    QVERIFY(!request.contains(QStringLiteral("is_blocked")));

    QCOMPARE(request.value(QStringLiteral("sender_id"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("messageSenderUser"));
}

void ChatBlockingTest::blocksChatSender()
{
    const QJsonObject request = blockSenderRequestChat(-1001234567890LL, true);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setMessageSenderBlockList"));

    const QJsonObject sender = request.value(QStringLiteral("sender_id")).toObject();
    QCOMPARE(sender.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderChat"));
    QCOMPARE(sender.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(-1001234567890LL));

    QCOMPARE(request.value(QStringLiteral("block_list"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("blockListMain"));

    const QJsonObject unblock = blockSenderRequestChat(-1001234567890LL, false);
    QVERIFY(unblock.value(QStringLiteral("block_list")).isNull());
    QCOMPARE(unblock.value(QStringLiteral("sender_id"))
                 .toObject()
                 .value(QStringLiteral("chat_id"))
                 .toVariant()
                 .toLongLong(),
             qint64(-1001234567890LL));
}

void ChatBlockingTest::reportMessageStartsWithEmptyOptionId()
{
    const QJsonObject request = reportMessageRequest(5, 77, QStringLiteral("spam"));

    // This schema has no reportMessage and reportChat has no ReportReason field.
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reportChat"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(5));
    QVERIFY(!request.contains(QStringLiteral("reason")));
    QVERIFY(!request.contains(QStringLiteral("message_id")));

    // The initial request carries no option yet, so TDLib answers with
    // reportChatResultOptionRequired.
    QVERIFY(request.value(QStringLiteral("option_id")).isString());
    QCOMPARE(request.value(QStringLiteral("option_id")).toString(), QString());
    // A known reason is chosen from Telegram's option list, never sent as text.
    QCOMPARE(request.value(QStringLiteral("text")).toString(), QString());

    const QJsonArray ids = request.value(QStringLiteral("message_ids")).toArray();
    QCOMPARE(ids.size(), 1);
    QCOMPARE(ids.at(0).toVariant().toLongLong(), qint64(77));
}

void ChatBlockingTest::reportMessageKeepsFreeFormReasonAsText()
{
    const QJsonObject typed =
        reportMessageRequest(5, 77, QStringLiteral("  scam links in bio  "));
    QCOMPARE(typed.value(QStringLiteral("text")).toString(),
             QStringLiteral("scam links in bio"));
    QCOMPARE(typed.value(QStringLiteral("option_id")).toString(), QString());
    QCOMPARE(typed.value(QStringLiteral("message_ids")).toArray().size(), 1);

    // Schema spellings, including a loose one, are recognised and dropped.
    QCOMPARE(reportMessageRequest(5, 77, QStringLiteral("reportReasonSpam"))
                 .value(QStringLiteral("text"))
                 .toString(),
             QString());
    QCOMPARE(reportMessageRequest(5, 77, QStringLiteral("personal details"))
                 .value(QStringLiteral("text"))
                 .toString(),
             QString());
    QCOMPARE(reportMessageRequest(5, 77, QString()).value(QStringLiteral("text")).toString(),
             QString());

    // reportChat.text is limited to 1024 characters.
    const QJsonObject longReason =
        reportMessageRequest(5, 77, QString(2000, QLatin1Char('x')));
    QCOMPARE(longReason.value(QStringLiteral("text")).toString().size(), 1024);
}

void ChatBlockingTest::mapsReasonNames()
{
    QCOMPARE(reportReasonType(QStringLiteral("spam")), QStringLiteral("reportReasonSpam"));
    QCOMPARE(reportReasonType(QStringLiteral("reportReasonSpam")),
             QStringLiteral("reportReasonSpam"));
    QCOMPARE(reportReasonType(QStringLiteral("ReportReasonSpam")),
             QStringLiteral("reportReasonSpam"));
    QCOMPARE(reportReasonType(QStringLiteral("personal details")),
             QStringLiteral("reportReasonPersonalDetails"));
    QCOMPARE(reportReasonType(QStringLiteral("personal_details")),
             QStringLiteral("reportReasonPersonalDetails"));
    QCOMPARE(reportReasonType(QStringLiteral("childAbuse")),
             QStringLiteral("reportReasonChildAbuse"));
    QCOMPARE(reportReasonType(QStringLiteral("unrelated-location")),
             QStringLiteral("reportReasonUnrelatedLocation"));
    QCOMPARE(reportReasonType(QStringLiteral("custom")), QStringLiteral("reportReasonCustom"));

    // Unknown or empty reasons map to nothing, so a typo is never sent.
    QCOMPARE(reportReasonType(QStringLiteral("harassment")), QString());
    QCOMPARE(reportReasonType(QStringLiteral("reportReasonNope")), QString());
    QCOMPARE(reportReasonType(QStringLiteral("reportReason")), QString());
    QCOMPARE(reportReasonType(QString()), QString());
    QCOMPARE(reportReasonType(QStringLiteral("   ")), QString());
}

void ChatBlockingTest::listsReasonTypes()
{
    const QStringList types = reportReasonTypes();
    QCOMPARE(types.size(), 10);

    const QStringList expected{
        QStringLiteral("reportReasonSpam"),
        QStringLiteral("reportReasonViolence"),
        QStringLiteral("reportReasonPornography"),
        QStringLiteral("reportReasonChildAbuse"),
        QStringLiteral("reportReasonCopyright"),
        QStringLiteral("reportReasonUnrelatedLocation"),
        QStringLiteral("reportReasonFake"),
        QStringLiteral("reportReasonIllegalDrugs"),
        QStringLiteral("reportReasonPersonalDetails"),
        QStringLiteral("reportReasonCustom"),
    };
    QCOMPARE(types, expected);

    for (const QString& type : types) {
        QVERIFY(type.startsWith(QStringLiteral("reportReason")));
        // Every listed name must round-trip through the mapping helper.
        QCOMPARE(reportReasonType(type), type);
    }
}

void ChatBlockingTest::picksReportOptionForReason()
{
    const QJsonArray options{
        reportOption(base64Id("other"), QStringLiteral("Other")),
        reportOption(base64Id("violence"), QStringLiteral("Violence")),
        reportOption(base64Id("spam"), QStringLiteral("Spam")),
    };

    // The chosen id is the value from the answer, passed through untouched.
    QCOMPARE(reportOptionIdForReason(options, QStringLiteral("spam")), base64Id("spam"));
    QCOMPARE(reportOptionIdForReason(options, QStringLiteral("reportReasonSpam")),
             base64Id("spam"));
    QCOMPARE(reportOptionIdForReason(options, QStringLiteral("violence")),
             base64Id("violence"));

    // Telegram localises the option text; the reason still has to match.
    const QJsonArray localized{
        reportOption(base64Id("opaque-1"), QStringLiteral("Спам")),
    };
    QCOMPARE(reportOptionIdForReason(localized, QStringLiteral("spam")),
             base64Id("opaque-1"));

    // A missing option is never replaced with an unrelated reason.
    QCOMPARE(reportOptionIdForReason(options, QStringLiteral("personalDetails")), QString());
    QCOMPARE(reportOptionIdForReason(options, QStringLiteral("custom")), QString());
    QCOMPARE(reportOptionIdForReason(options, QString()), QString());
    QCOMPARE(reportOptionIdForReason(QJsonArray{}, QStringLiteral("spam")), QString());
}

void ChatBlockingTest::reportOptionRequestCarriesChosenOption()
{
    const QString chosen = base64Id("spam");
    const QJsonObject request =
        reportMessageOptionRequest(5, 77, chosen, QStringLiteral("details"));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reportChat"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(5));
    // Bytes travel base64-encoded, so the option id is not re-encoded.
    QCOMPARE(request.value(QStringLiteral("option_id")).toString(), chosen);
    QCOMPARE(request.value(QStringLiteral("text")).toString(), QStringLiteral("details"));

    const QJsonArray ids = request.value(QStringLiteral("message_ids")).toArray();
    QCOMPARE(ids.size(), 1);
    QCOMPARE(ids.at(0).toVariant().toLongLong(), qint64(77));

    // Confirming an optional text step sends no text.
    const QJsonObject confirm = reportMessageOptionRequest(5, 77, chosen);
    QCOMPARE(confirm.value(QStringLiteral("text")).toString(), QString());
    QCOMPARE(confirm.value(QStringLiteral("option_id")).toString(), chosen);
}

void ChatBlockingTest::clampsBlockedSendersPaging()
{
    QCOMPARE(getBlockedSendersRequest(-5, 20).value(QStringLiteral("offset")).toInt(), 0);
    QCOMPARE(getBlockedSendersRequest(0, 20).value(QStringLiteral("offset")).toInt(), 0);
    QCOMPARE(getBlockedSendersRequest(250, 20).value(QStringLiteral("offset")).toInt(), 250);

    QCOMPARE(getBlockedSendersRequest(0, 0).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(getBlockedSendersRequest(0, -3).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(getBlockedSendersRequest(0, 1).value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(getBlockedSendersRequest(0, 100).value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(getBlockedSendersRequest(0, 101).value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(getBlockedSendersRequest(0, 5000).value(QStringLiteral("limit")).toInt(), 100);
}

void ChatBlockingTest::blockedSendersRequestShape()
{
    const QJsonObject request = getBlockedSendersRequest(10, 50);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getBlockedMessageSenders"));
    QCOMPARE(request.value(QStringLiteral("offset")).toInt(), 10);
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);

    // The schema requires a concrete block list; this client uses the main one.
    const QJsonValue blockList = request.value(QStringLiteral("block_list"));
    QVERIFY(blockList.isObject());
    QCOMPARE(blockList.toObject().value(QStringLiteral("@type")).toString(),
             QStringLiteral("blockListMain"));
    QCOMPARE(blockList.toObject().size(), 1);
}

void ChatBlockingTest::detectsMessageSendersResponse()
{
    QVERIFY(responseIsMessageSenders(
        parseObject(R"({"@type": "messageSenders", "total_count": 0, "senders": []})")));
    QVERIFY(responseIsMessageSenders(parseObject(R"({"@type": "messageSenders"})")));
    QVERIFY(!responseIsMessageSenders(QJsonObject{}));
    QVERIFY(!responseIsMessageSenders(parseObject(R"({"@type": "ok"})")));
    QVERIFY(!responseIsMessageSenders(
        parseObject(R"({"@type": "error", "message": "404"})")));
    QVERIFY(!responseIsMessageSenders(
        parseObject(R"({"@type": "messageSenderUser", "user_id": 1})")));
}

void ChatBlockingTest::blocksSenderFromReplies()
{
    const QJsonObject request =
        blockSenderFromRepliesRequest(88, true, true, false);

    QCOMPARE(request.size(), 5);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("blockMessageSenderFromReplies"));
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(), qint64(88));
    QCOMPARE(request.value(QStringLiteral("delete_message")).toBool(), true);
    QCOMPARE(request.value(QStringLiteral("delete_all_messages")).toBool(), true);
    QCOMPARE(request.value(QStringLiteral("report_spam")).toBool(), false);

    // Every flag is explicit: TDLib requires all four fields.
    const QJsonObject quiet = blockSenderFromRepliesRequest(9, false, false, false);
    QCOMPARE(quiet.value(QStringLiteral("delete_message")).toBool(), false);
    QCOMPARE(quiet.value(QStringLiteral("delete_all_messages")).toBool(), false);
    QCOMPARE(quiet.value(QStringLiteral("report_spam")).toBool(), false);
}

void ChatBlockingTest::requestsSurviveSerialization()
{
    const QJsonObject blocked = roundTrip(blockSenderRequest(42, true));
    QCOMPARE(blocked.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setMessageSenderBlockList"));
    QCOMPARE(blocked.value(QStringLiteral("sender_id"))
                 .toObject()
                 .value(QStringLiteral("user_id"))
                 .toVariant()
                 .toLongLong(),
             qint64(42));
    QCOMPARE(blocked.value(QStringLiteral("block_list"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("blockListMain"));

    // The null block list must stay null through the transport encoding.
    const QJsonObject unblocked = roundTrip(blockSenderRequestChat(-1001234567890LL, false));
    QVERIFY(unblocked.value(QStringLiteral("block_list")).isNull());

    const QJsonObject report =
        roundTrip(reportMessageRequest(5, 77, QStringLiteral("scam links")));
    QCOMPARE(report.value(QStringLiteral("option_id")).toString(), QString());
    QCOMPARE(report.value(QStringLiteral("text")).toString(), QStringLiteral("scam links"));
    QCOMPARE(report.value(QStringLiteral("message_ids")).toArray().size(), 1);
    QCOMPARE(report.value(QStringLiteral("message_ids"))
                 .toArray()
                 .at(0)
                 .toVariant()
                 .toLongLong(),
             qint64(77));

    const QJsonObject paged = roundTrip(getBlockedSendersRequest(20, 40));
    QCOMPARE(paged.value(QStringLiteral("offset")).toInt(), 20);
    QCOMPARE(paged.value(QStringLiteral("limit")).toInt(), 40);
    QCOMPARE(paged.value(QStringLiteral("block_list"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("blockListMain"));
}

QTEST_GUILESS_MAIN(ChatBlockingTest)
#include "tst_chat_blocking.moc"
