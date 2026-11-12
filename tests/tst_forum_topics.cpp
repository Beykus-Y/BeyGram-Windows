#include "telegram/ForumTopics.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

qint64 int64Field(const QJsonObject& object, const char* key)
{
    return object.value(QLatin1String(key)).toVariant().toLongLong();
}

} // namespace

class ForumTopicsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsTopicsPageRequest();
    void clampsTopicsPageRequest();
    void rejectsInvalidTopicsPageRequest();
    void buildsTopicRequest();
    void buildsPinnedToggle();
    void buildsClosedToggle();
    void rejectsInvalidTopicIds();
    void buildsCreateRequest();
    void trimsAndValidatesTitles();
    void countsTitleInCodePoints();
    void buildsEditRequest();
    void parsesTopicsInServerOrder();
    void skipsMalformedTopics();
    void acceptsFlattenedFlagsAndStringNumbers();
    void ignoresForeignResponses();
};

void ForumTopicsTest::buildsTopicsPageRequest()
{
    const QJsonObject request = getForumTopicsRequest(100, 50, 1700000000, 42);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getForumTopics"));
    QCOMPARE(int64Field(request, "chat_id"), qint64(100));
    QVERIFY(request.value(QStringLiteral("query")).isString());
    QCOMPARE(request.value(QStringLiteral("query")).toString(), QString());
    QCOMPARE(int64Field(request, "offset_date"), qint64(1700000000));
    QCOMPARE(int64Field(request, "offset_message_id"), qint64(42));
    QCOMPARE(int64Field(request, "offset_forum_topic_id"), qint64(0));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void ForumTopicsTest::clampsTopicsPageRequest()
{
    const QJsonObject small = getForumTopicsRequest(1, 0, -5, -6);
    QCOMPARE(small.value(QStringLiteral("limit")).toInt(), 1);
    QCOMPARE(int64Field(small, "offset_date"), qint64(0));
    QCOMPARE(int64Field(small, "offset_message_id"), qint64(0));

    const QJsonObject large = getForumTopicsRequest(1, 500, 9999999999LL, 9999999999LL);
    QCOMPARE(large.value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(int64Field(large, "offset_date"),
             qint64(std::numeric_limits<int>::max()));
    QCOMPARE(int64Field(large, "offset_message_id"), qint64(9999999999LL));
}

void ForumTopicsTest::rejectsInvalidTopicsPageRequest()
{
    QVERIFY(getForumTopicsRequest(0, 50, 0, 0).isEmpty());
    QVERIFY(getForumTopicsRequest(-42, 50, 0, 0).isEmpty());
}

void ForumTopicsTest::buildsTopicRequest()
{
    const QJsonObject request = getForumTopicRequest(100, 11);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getForumTopic"));
    QCOMPARE(int64Field(request, "chat_id"), qint64(100));
    QCOMPARE(int64Field(request, "forum_topic_id"), qint64(11));
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void ForumTopicsTest::buildsPinnedToggle()
{
    const QJsonObject pinned = toggleForumTopicIsPinnedRequest(100, 11, true);
    QCOMPARE(pinned.value(QStringLiteral("@type")).toString(),
             QStringLiteral("toggleForumTopicIsPinned"));
    QCOMPARE(int64Field(pinned, "chat_id"), qint64(100));
    QCOMPARE(int64Field(pinned, "forum_topic_id"), qint64(11));
    QVERIFY(pinned.value(QStringLiteral("is_pinned")).isBool());
    QCOMPARE(pinned.value(QStringLiteral("is_pinned")).toBool(), true);

    const QJsonObject unpinned = toggleForumTopicIsPinnedRequest(100, 11, false);
    QVERIFY(unpinned.value(QStringLiteral("is_pinned")).isBool());
    QCOMPARE(unpinned.value(QStringLiteral("is_pinned")).toBool(), false);
}

void ForumTopicsTest::buildsClosedToggle()
{
    const QJsonObject closed = toggleForumTopicIsClosedRequest(100, 11, true);
    QCOMPARE(closed.value(QStringLiteral("@type")).toString(),
             QStringLiteral("toggleForumTopicIsClosed"));
    QCOMPARE(int64Field(closed, "chat_id"), qint64(100));
    QCOMPARE(int64Field(closed, "forum_topic_id"), qint64(11));
    QVERIFY(closed.value(QStringLiteral("is_closed")).isBool());
    QCOMPARE(closed.value(QStringLiteral("is_closed")).toBool(), true);

    const QJsonObject reopened = toggleForumTopicIsClosedRequest(100, 11, false);
    QVERIFY(reopened.value(QStringLiteral("is_closed")).isBool());
    QCOMPARE(reopened.value(QStringLiteral("is_closed")).toBool(), false);
}

void ForumTopicsTest::rejectsInvalidTopicIds()
{
    QVERIFY(getForumTopicRequest(0, 11).isEmpty());
    QVERIFY(getForumTopicRequest(100, 0).isEmpty());
    QVERIFY(getForumTopicRequest(100, -1).isEmpty());

    QVERIFY(toggleForumTopicIsPinnedRequest(0, 11, true).isEmpty());
    QVERIFY(toggleForumTopicIsPinnedRequest(100, 0, true).isEmpty());
    QVERIFY(toggleForumTopicIsClosedRequest(0, 11, true).isEmpty());
    QVERIFY(toggleForumTopicIsClosedRequest(100, -7, false).isEmpty());

    QVERIFY(createForumTopicRequest(0, QStringLiteral("Title")).isEmpty());
    QVERIFY(createForumTopicRequest(-1, QStringLiteral("Title")).isEmpty());
    QVERIFY(editForumTopicRequest(0, 11, QStringLiteral("Title")).isEmpty());
    QVERIFY(editForumTopicRequest(100, 0, QStringLiteral("Title")).isEmpty());
}

void ForumTopicsTest::buildsCreateRequest()
{
    const QJsonObject request = createForumTopicRequest(100, QStringLiteral("  Release notes  "));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("createForumTopic"));
    QCOMPARE(int64Field(request, "chat_id"), qint64(100));
    QCOMPARE(request.value(QStringLiteral("name")).toString(),
             QStringLiteral("Release notes"));
    QVERIFY(request.value(QStringLiteral("is_name_implicit")).isBool());
    QCOMPARE(request.value(QStringLiteral("is_name_implicit")).toBool(), false);

    const QJsonObject icon = request.value(QStringLiteral("icon")).toObject();
    QCOMPARE(icon.value(QStringLiteral("@type")).toString(),
             QStringLiteral("forumTopicIcon"));
    QCOMPARE(icon.value(QStringLiteral("color")).toInt(), 0x6FB9F0);
    QCOMPARE(int64Field(icon, "custom_emoji_id"), qint64(0));
}

void ForumTopicsTest::trimsAndValidatesTitles()
{
    QVERIFY(createForumTopicRequest(1, QString()).isEmpty());
    QVERIFY(createForumTopicRequest(1, QStringLiteral("   \t\n ")).isEmpty());

    const QJsonObject created = createForumTopicRequest(1, QStringLiteral("  General  "));
    QCOMPARE(created.value(QStringLiteral("name")).toString(), QStringLiteral("General"));

    QVERIFY(editForumTopicRequest(1, 2, QString()).isEmpty());
    QVERIFY(editForumTopicRequest(1, 2, QStringLiteral("  ")).isEmpty());

    const QJsonObject edited = editForumTopicRequest(1, 2, QStringLiteral("  Renamed  "));
    QCOMPARE(edited.value(QStringLiteral("name")).toString(), QStringLiteral("Renamed"));
}

void ForumTopicsTest::countsTitleInCodePoints()
{
    // U+1F600 is one code point but two UTF-16 units. Telegram counts the
    // former, so 128 emoji are a valid name even though QString::size() is 256.
    const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
    QString maxTitle;
    maxTitle.reserve(256);
    for (int index = 0; index < 128; ++index)
        maxTitle += emoji;

    QCOMPARE(static_cast<int>(maxTitle.size()), 256);
    QCOMPARE(createForumTopicRequest(1, maxTitle).value(QStringLiteral("name")).toString(),
             maxTitle);

    const QString tooLong = maxTitle + emoji;
    QVERIFY(createForumTopicRequest(1, tooLong).isEmpty());
    QVERIFY(editForumTopicRequest(1, 2, tooLong).isEmpty());
}

void ForumTopicsTest::buildsEditRequest()
{
    const QJsonObject request = editForumTopicRequest(100, 11, QStringLiteral("  Renamed  "));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("editForumTopic"));
    QCOMPARE(int64Field(request, "chat_id"), qint64(100));
    QCOMPARE(int64Field(request, "forum_topic_id"), qint64(11));
    QCOMPARE(request.value(QStringLiteral("name")).toString(), QStringLiteral("Renamed"));
    QVERIFY(request.value(QStringLiteral("edit_icon_custom_emoji")).isBool());
    QCOMPARE(request.value(QStringLiteral("edit_icon_custom_emoji")).toBool(), false);
    QCOMPARE(int64Field(request, "icon_custom_emoji_id"), qint64(0));
}

void ForumTopicsTest::parsesTopicsInServerOrder()
{
    const QJsonObject response = parseObject(R"JSON(
        {
            "@type": "forumTopics",
            "total_count": 3,
            "topics": [
                {
                    "@type": "forumTopic",
                    "info": {
                        "@type": "forumTopicInfo",
                        "chat_id": 100,
                        "forum_topic_id": 11,
                        "name": "Announcements",
                        "is_closed": false,
                        "icon": {
                            "@type": "forumTopicIcon",
                            "color": 7322096,
                            "custom_emoji_id": "5368324170671202286"
                        }
                    },
                    "last_message": {
                        "@type": "message",
                        "id": 900,
                        "date": 1700000100
                    },
                    "unread_count": 4,
                    "is_pinned": true
                },
                {
                    "@type": "forumTopic",
                    "info": {
                        "@type": "forumTopicInfo",
                        "forum_topic_id": 12,
                        "name": "Closed topic",
                        "is_closed": true
                    },
                    "last_message": null,
                    "unread_count": 0,
                    "is_pinned": false
                },
                {
                    "@type": "forumTopic",
                    "info": {
                        "@type": "forumTopicInfo",
                        "forum_topic_id": 13,
                        "name": "General"
                    },
                    "last_message": {
                        "@type": "message",
                        "date": 1700000200
                    },
                    "unread_count": 1,
                    "is_pinned": false
                }
            ]
        })JSON");

    const QVector<ForumTopicRow> rows = forumTopicsFromResponse(response);
    QCOMPARE(rows.size(), 3);

    QCOMPARE(rows.at(0).topicId, qint64(11));
    QCOMPARE(rows.at(0).title, QStringLiteral("Announcements"));
    QCOMPARE(rows.at(0).unreadCount, 4);
    QVERIFY(rows.at(0).pinned);
    QVERIFY(!rows.at(0).closed);
    QCOMPARE(rows.at(0).lastMessageDate, qint64(1700000100));
    QCOMPARE(rows.at(0).iconFileId, qint64(5368324170671202286LL));

    QCOMPARE(rows.at(1).topicId, qint64(12));
    QCOMPARE(rows.at(1).title, QStringLiteral("Closed topic"));
    QCOMPARE(rows.at(1).unreadCount, 0);
    QVERIFY(!rows.at(1).pinned);
    QVERIFY(rows.at(1).closed);
    QCOMPARE(rows.at(1).lastMessageDate, qint64(0));
    QCOMPARE(rows.at(1).iconFileId, qint64(0));

    QCOMPARE(rows.at(2).topicId, qint64(13));
    QCOMPARE(rows.at(2).title, QStringLiteral("General"));
    QCOMPARE(rows.at(2).unreadCount, 1);
    QVERIFY(!rows.at(2).pinned);
    QVERIFY(!rows.at(2).closed);
    QCOMPARE(rows.at(2).lastMessageDate, qint64(1700000200));
}

void ForumTopicsTest::skipsMalformedTopics()
{
    const QJsonObject response = parseObject(R"JSON(
        {
            "@type": "forumTopics",
            "topics": [
                null,
                "not an object",
                {"@type": "forumTopic", "info": {"name": "No id"}},
                {"@type": "forumTopic", "info": {"forum_topic_id": 0, "name": "Zero"}},
                {"@type": "forumTopic", "info": {"forum_topic_id": -5, "name": "Negative"}},
                {"@type": "forumTopic", "info": {"forum_topic_id": 21, "name": "   "}},
                {"@type": "forumTopic", "info": {"forum_topic_id": 22}},
                {"@type": "forumTopic", "info": {"forum_topic_id": 23, "name": "First valid"}},
                {"@type": "forumTopic", "info": {"forum_topic_id": 24, "name": "Second valid"}}
            ]
        })JSON");

    const QVector<ForumTopicRow> rows = forumTopicsFromResponse(response);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).topicId, qint64(23));
    QCOMPARE(rows.at(0).title, QStringLiteral("First valid"));
    QCOMPARE(rows.at(1).topicId, qint64(24));
    QCOMPARE(rows.at(1).title, QStringLiteral("Second valid"));
}

void ForumTopicsTest::acceptsFlattenedFlagsAndStringNumbers()
{
    const QJsonObject response = parseObject(R"JSON(
        {
            "@type": "forumTopics",
            "topics": [
                {
                    "@type": "forumTopic",
                    "topic_id": "31",
                    "title": "Flattened",
                    "unread_count": "7",
                    "is_pinned": true,
                    "is_closed": true,
                    "last_message": {"date": "1700000300"},
                    "icon_custom_emoji_id": "5368324170671202286"
                },
                {
                    "@type": "forumTopic",
                    "info": {
                        "@type": "forumTopicInfo",
                        "forum_topic_id": "32",
                        "name": "Info flags",
                        "is_pinned": true,
                        "is_closed": true,
                        "icon_custom_emoji_id": "777"
                    }
                }
            ]
        })JSON");

    const QVector<ForumTopicRow> rows = forumTopicsFromResponse(response);
    QCOMPARE(rows.size(), 2);

    QCOMPARE(rows.at(0).topicId, qint64(31));
    QCOMPARE(rows.at(0).title, QStringLiteral("Flattened"));
    QCOMPARE(rows.at(0).unreadCount, 7);
    QVERIFY(rows.at(0).pinned);
    QVERIFY(rows.at(0).closed);
    QCOMPARE(rows.at(0).lastMessageDate, qint64(1700000300));
    QCOMPARE(rows.at(0).iconFileId, qint64(5368324170671202286LL));

    QCOMPARE(rows.at(1).topicId, qint64(32));
    QCOMPARE(rows.at(1).title, QStringLiteral("Info flags"));
    QVERIFY(rows.at(1).pinned);
    QVERIFY(rows.at(1).closed);
    QCOMPARE(rows.at(1).iconFileId, qint64(777));
}

void ForumTopicsTest::ignoresForeignResponses()
{
    QVERIFY(forumTopicsFromResponse(QJsonObject{}).isEmpty());
    QVERIFY(forumTopicsFromResponse(parseObject(R"({
        "@type": "error",
        "code": 400,
        "message": "Bad Request"
    })")).isEmpty());
    QVERIFY(forumTopicsFromResponse(parseObject(R"({
        "@type": "forumTopics"
    })")).isEmpty());
    QVERIFY(forumTopicsFromResponse(parseObject(R"({
        "@type": "forumTopics",
        "topics": {}
    })")).isEmpty());
    QVERIFY(forumTopicsFromResponse(parseObject(R"({
        "@type": "messages",
        "topics": [
            {"info": {"forum_topic_id": 1, "name": "Not a forumTopics response"}}
        ]
    })")).isEmpty());
}

QTEST_GUILESS_MAIN(ForumTopicsTest)
#include "tst_forum_topics.moc"
