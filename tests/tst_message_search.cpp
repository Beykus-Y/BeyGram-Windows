#include "telegram/MessageSearch.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject jsonObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

QJsonObject messageTextEntry(qint64 id, qint64 chatId, qint64 date, const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("message")},
        {QStringLiteral("id"), id},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("date"), date},
        {QStringLiteral("sender_id"),
         QJsonObject{{QStringLiteral("@type"), QStringLiteral("messageSenderUser")},
                     {QStringLiteral("user_id"), 7}}},
        {QStringLiteral("content"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("messageText")},
             {QStringLiteral("text"),
              QJsonObject{{QStringLiteral("@type"), QStringLiteral("formattedText")},
                          {QStringLiteral("text"), text},
                          {QStringLiteral("entities"), QJsonArray()}}},
         }},
    };
}

} // namespace

class MessageSearchTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsInChatRequest();
    void clampsInChatLimit();
    void buildsGlobalRequest();
    void parsesTextAndCaptionHits();
    void parsesNestedFoundContainers();
    void leavesMissingTitlesEmpty();
    void skipsMalformedEntries();
    void reportsMoreResults();
};

void MessageSearchTest::buildsInChatRequest()
{
    const QJsonObject request =
        searchChatMessagesRequest(42, QStringLiteral("needle"), 1000, 50);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("searchChatMessages"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));
    QCOMPARE(request.value(QStringLiteral("query")).toString(), QStringLiteral("needle"));
    QCOMPARE(request.value(QStringLiteral("from_message_id")).toVariant().toLongLong(),
             qint64(1000));
    QCOMPARE(request.value(QStringLiteral("offset")).toInt(), 0);
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);

    // Current TDLib schema uses nullable topic_id/sender_id instead of the
    // older message_thread_id/sender_user_id/sender_chat_id/save_reaction.
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(request.value(QStringLiteral("sender_id")).isNull());
    QVERIFY(request.value(QStringLiteral("filter")).isNull());
    QVERIFY(!request.contains(QStringLiteral("message_thread_id")));
    QVERIFY(!request.contains(QStringLiteral("save_reaction")));
}

void MessageSearchTest::clampsInChatLimit()
{
    QCOMPARE(searchChatMessagesRequest(1, QStringLiteral("q"), 0, 0)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1);
    QCOMPARE(searchChatMessagesRequest(1, QStringLiteral("q"), 0, -10)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1);
    QCOMPARE(searchChatMessagesRequest(1, QStringLiteral("q"), 0, 1)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1);
    QCOMPARE(searchChatMessagesRequest(1, QStringLiteral("q"), 0, 100)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             100);
    QCOMPARE(searchChatMessagesRequest(1, QStringLiteral("q"), 0, 101)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             100);
    QCOMPARE(searchChatMessagesRequest(1, QStringLiteral("q"), 0, 5000)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             100);
}

void MessageSearchTest::buildsGlobalRequest()
{
    const QJsonObject request =
        searchMessagesRequest(QStringLiteral("cat videos"), 777, 250);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("searchMessages"));
    QVERIFY(request.value(QStringLiteral("chat_list")).isNull());
    QCOMPARE(request.value(QStringLiteral("query")).toString(),
             QStringLiteral("cat videos"));
    QVERIFY(request.value(QStringLiteral("offset")).isString());
    QVERIFY(request.value(QStringLiteral("offset")).toString().isEmpty());
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 100);
    QVERIFY(request.value(QStringLiteral("filter")).isNull());
    QVERIFY(request.value(QStringLiteral("chat_type_filter")).isNull());
    QCOMPARE(request.value(QStringLiteral("min_date")).toInt(), 0);
    QCOMPARE(request.value(QStringLiteral("max_date")).toInt(), 0);
    QVERIFY(!request.contains(QStringLiteral("from_message_id")));

    const QJsonObject clamped = searchMessagesRequest(QStringLiteral("q"), 0, 0);
    QCOMPARE(clamped.value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(searchMessagesRequest(QStringLiteral("q"), 0, 101)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             100);
}

void MessageSearchTest::parsesTextAndCaptionHits()
{
    const QJsonObject response = jsonObject(R"json({
        "@type": "foundChatMessages",
        "total_count": 2,
        "next_from_message_id": 900,
        "messages": [
            {
                "@type": "message",
                "id": 901,
                "chat_id": -100123,
                "date": 1700000000,
                "sender_id": {"@type": "messageSenderUser", "user_id": 7},
                "content": {
                    "@type": "messageText",
                    "text": {
                        "@type": "formattedText",
                        "text": "a <b>bold</b> & \"quoted\"",
                        "entities": []
                    }
                }
            },
            {
                "@type": "message",
                "id": 900,
                "chat_id": -100123,
                "date": 1699999990,
                "sender_id": {"@type": "messageSenderChat", "chat_id": -100123},
                "content": {
                    "@type": "messagePhoto",
                    "photo": {"sizes": []},
                    "caption": {
                        "@type": "formattedText",
                        "text": "cap <i>here</i>",
                        "entities": []
                    }
                }
            }
        ]
    })json");

    const QVector<SearchHit> hits = searchHitsFromResponse(response);
    QCOMPARE(hits.size(), 2);

    const SearchHit& textHit = hits.at(0);
    QCOMPARE(textHit.chatId, qint64(-100123));
    QCOMPARE(textHit.messageId, qint64(901));
    QCOMPARE(textHit.date, qint64(1700000000));
    QCOMPARE(textHit.text, QStringLiteral("a <b>bold</b> & \"quoted\""));
    QVERIFY(!textHit.textHtml.contains(QStringLiteral("<b>")));
    QVERIFY(textHit.textHtml.contains(QStringLiteral("&lt;b&gt;bold&lt;/b&gt;")));
    QVERIFY(textHit.textHtml.contains(QStringLiteral("&amp;")));
    QVERIFY(textHit.textHtml.contains(QStringLiteral("&quot;quoted&quot;")));
    QVERIFY(textHit.chatTitle.isEmpty());
    QVERIFY(textHit.senderName.isEmpty());

    const SearchHit& captionHit = hits.at(1);
    QCOMPARE(captionHit.messageId, qint64(900));
    QCOMPARE(captionHit.text, QStringLiteral("cap <i>here</i>"));
    QVERIFY(captionHit.textHtml.contains(QStringLiteral("cap &lt;i&gt;here&lt;/i&gt;")));
    QVERIFY(!captionHit.textHtml.contains(QStringLiteral("<i>")));
}

void MessageSearchTest::parsesNestedFoundContainers()
{
    const QJsonObject response = jsonObject(R"json({
        "@type": "foundChatMessages",
        "messages": [
            {
                "@type": "foundChatMessages",
                "total_count": 1,
                "messages": [
                    {
                        "@type": "message",
                        "id": 12,
                        "chat_id": 34,
                        "date": 100,
                        "content": {
                            "@type": "messageText",
                            "text": {"@type": "formattedText", "text": "nested",
                                     "entities": []}
                        }
                    }
                ]
            },
            {"@type": "foundChatMessages", "total_count": 0, "messages": []}
        ]
    })json");

    const QVector<SearchHit> hits = searchHitsFromResponse(response);
    QCOMPARE(hits.size(), 1);
    QCOMPARE(hits.at(0).messageId, qint64(12));
    QCOMPARE(hits.at(0).chatId, qint64(34));
    QCOMPARE(hits.at(0).text, QStringLiteral("nested"));
}

void MessageSearchTest::leavesMissingTitlesEmpty()
{
    QJsonObject enriched = messageTextEntry(5, 6, 7, QStringLiteral("hi"));
    enriched.insert(QStringLiteral("chat_title"), QStringLiteral("Saved"));
    enriched.insert(QStringLiteral("sender_name"), QStringLiteral("Ada"));
    QJsonObject signedPost = messageTextEntry(8, 6, 9, QStringLiteral("post"));
    signedPost.insert(QStringLiteral("author_signature"), QStringLiteral("Channel"));

    const QJsonArray messages{messageTextEntry(1, 2, 3, QStringLiteral("plain")), enriched,
                              signedPost};
    const QVector<SearchHit> hits =
        searchHitsFromResponse(QJsonObject{{QStringLiteral("messages"), messages}});

    QCOMPARE(hits.size(), 3);
    QVERIFY(hits.at(0).chatTitle.isEmpty());
    QVERIFY(hits.at(0).senderName.isEmpty());
    QCOMPARE(hits.at(1).chatTitle, QStringLiteral("Saved"));
    QCOMPARE(hits.at(1).senderName, QStringLiteral("Ada"));
    QCOMPARE(hits.at(2).senderName, QStringLiteral("Channel"));
}

void MessageSearchTest::skipsMalformedEntries()
{
    QVERIFY(searchHitsFromResponse(QJsonObject{}).isEmpty());
    QVERIFY(searchHitsFromResponse(jsonObject(R"json({"messages": null})json")).isEmpty());
    QVERIFY(searchHitsFromResponse(jsonObject(R"json({"messages": "nope"})json")).isEmpty());
    QVERIFY(searchHitsFromResponse(jsonObject(R"json({"total_count": 3})json")).isEmpty());

    const QJsonObject response = jsonObject(R"json({
        "messages": [
            null,
            5,
            "message",
            {},
            {"@type": "message"},
            {"@type": "message", "id": 0, "chat_id": 1},
            {"@type": "chat", "id": 4},
            {"@type": "foundChatMessages"},
            {
                "@type": "message",
                "id": "12",
                "chat_id": "34",
                "date": "56",
                "content": {
                    "@type": "messageText",
                    "text": {"@type": "formattedText", "text": "ok", "entities": []}
                }
            }
        ]
    })json");

    const QVector<SearchHit> hits = searchHitsFromResponse(response);
    QCOMPARE(hits.size(), 1);
    QCOMPARE(hits.at(0).messageId, qint64(12));
    QCOMPARE(hits.at(0).chatId, qint64(34));
    QCOMPARE(hits.at(0).date, qint64(56));
    QCOMPARE(hits.at(0).text, QStringLiteral("ok"));
}

void MessageSearchTest::reportsMoreResults()
{
    QVERIFY(searchResponseHasMore(jsonObject(
        R"json({"@type": "foundChatMessages", "next_from_message_id": 123})json")));
    QVERIFY(searchResponseHasMore(jsonObject(
        R"json({"@type": "foundChatMessages", "next_from_message_id": "123"})json")));
    QVERIFY(!searchResponseHasMore(jsonObject(
        R"json({"@type": "foundChatMessages", "next_from_message_id": 0})json")));
    QVERIFY(!searchResponseHasMore(jsonObject(
        R"json({"@type": "foundChatMessages", "total_count": 5})json")));
    QVERIFY(!searchResponseHasMore(QJsonObject{}));

    QVERIFY(searchResponseHasMore(
        jsonObject(R"json({"@type": "foundMessages", "next_offset": "opaque"})json")));
    QVERIFY(!searchResponseHasMore(
        jsonObject(R"json({"@type": "foundMessages", "next_offset": ""})json")));
}

QTEST_GUILESS_MAIN(MessageSearchTest)
#include "tst_message_search.moc"
