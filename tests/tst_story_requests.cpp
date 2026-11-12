#include "telegram/StoryRequests.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject objectFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

} // namespace

class StoryRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsGetChatActiveStoriesRequest();
    void buildsGetStoryRequest();
    void buildsOpenAndCloseStoryRequests();
    void preservesLargePosterChatIds();
    void parsesChatActiveStories();
    void unwrapsUpdateChatActiveStories();
    void parsesStoriesContainer();
    void parsesSingleStory();
    void readsPinnedStoryIds();
    void readsLargestPhotoSize();
    void readsMainVideoFileId();
    void handlesUnsupportedAndMissingContent();
    void keepsUnreadStoryUnread();
    void toleratesMissingAndWronglyTypedFields();
    void parsesStringEncodedIds();
    void skipsEntriesWithoutStoryId();
    void emptyResponsesYieldNothing();
    void requestsRoundTripThroughJson();
};

void StoryRequestsTest::buildsGetChatActiveStoriesRequest()
{
    const QJsonObject request = serialized(getChatActiveStoriesRequest(42));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatActiveStories"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(int(request.size()), 2);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void StoryRequestsTest::buildsGetStoryRequest()
{
    const QJsonObject request = serialized(getStoryRequest(-1001234567890LL, 7));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getStory"));
    QCOMPARE(numberAt(request, QStringLiteral("story_poster_chat_id")),
             qint64(-1001234567890LL));
    QCOMPARE(numberAt(request, QStringLiteral("story_id")), qint64(7));
    // only_local is required by the schema and this builder always asks for the
    // server copy.
    QVERIFY(request.value(QStringLiteral("only_local")).isBool());
    QVERIFY(!request.value(QStringLiteral("only_local")).toBool());
    QCOMPARE(int(request.size()), 4);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void StoryRequestsTest::buildsOpenAndCloseStoryRequests()
{
    const QJsonObject open = serialized(openStoryRequest(42, 7));
    QCOMPARE(open.value(QStringLiteral("@type")).toString(),
             QStringLiteral("openStory"));
    QCOMPARE(numberAt(open, QStringLiteral("story_poster_chat_id")), qint64(42));
    QCOMPARE(numberAt(open, QStringLiteral("story_id")), qint64(7));
    QCOMPARE(int(open.size()), 3);
    QVERIFY(!open.contains(QStringLiteral("only_local")));
    QVERIFY(!open.contains(QStringLiteral("@extra")));

    const QJsonObject close = serialized(closeStoryRequest(-1001234567890LL, 8));
    QCOMPARE(close.value(QStringLiteral("@type")).toString(),
             QStringLiteral("closeStory"));
    QCOMPARE(numberAt(close, QStringLiteral("story_poster_chat_id")),
             qint64(-1001234567890LL));
    QCOMPARE(numberAt(close, QStringLiteral("story_id")), qint64(8));
    QCOMPARE(int(close.size()), 3);
    QVERIFY(!close.contains(QStringLiteral("only_local")));
    QVERIFY(!close.contains(QStringLiteral("@extra")));
}

void StoryRequestsTest::preservesLargePosterChatIds()
{
    // Telegram chat ids exceed 32 bits; they must not be truncated.
    const qint64 chatId = -1001234567890LL;

    const QJsonObject request = serialized(getChatActiveStoriesRequest(chatId));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);

    const QJsonObject response = objectFromJson(R"({
        "@type": "chatActiveStories",
        "chat_id": -1001234567890,
        "max_read_story_id": 0,
        "stories": [{"@type": "storyInfo", "story_id": 1, "date": 1}]
    })");
    const QVector<StoryInfo> stories = storiesFromResponse(response);
    QCOMPARE(stories.size(), 1);
    QCOMPARE(stories.first().posterChatId, chatId);
}

void StoryRequestsTest::parsesChatActiveStories()
{
    const QJsonObject response = objectFromJson(R"({
        "@type": "chatActiveStories",
        "chat_id": 42,
        "list": {"@type": "storyListMain"},
        "order": 100,
        "can_be_archived": false,
        "max_read_story_id": 5,
        "stories": [
            {"@type": "storyInfo", "story_id": 4, "date": 1700000000,
             "is_for_close_friends": false, "is_live": false},
            {"@type": "storyInfo", "story_id": 5, "date": 1700000100,
             "is_for_close_friends": true, "is_live": false},
            {"@type": "storyInfo", "story_id": 6, "date": 1700000200,
             "is_for_close_friends": false, "is_live": false}
        ]
    })");

    const QVector<StoryInfo> stories = storiesFromResponse(response);

    QCOMPARE(stories.size(), 3);
    QCOMPARE(stories[0].posterChatId, qint64(42));
    QCOMPARE(stories[0].id, 4);
    QCOMPARE(stories[0].date, qint64(1700000000));
    QVERIFY(stories[0].isViewed);
    QVERIFY(!stories[0].isPinned);
    QCOMPARE(stories[0].photoFileId, 0);
    QCOMPARE(stories[0].videoFileId, 0);

    QCOMPARE(stories[1].id, 5);
    QCOMPARE(stories[1].date, qint64(1700000100));
    QVERIFY(stories[1].isViewed);
    QVERIFY(!stories[1].isPinned);

    // max_read_story_id 5 is the last read story, so 6 is still unread.
    QCOMPARE(stories[2].id, 6);
    QVERIFY(!stories[2].isViewed);
}

void StoryRequestsTest::unwrapsUpdateChatActiveStories()
{
    const QJsonObject update = objectFromJson(R"({
        "@type": "updateChatActiveStories",
        "active_stories": {
            "@type": "chatActiveStories",
            "chat_id": 42,
            "max_read_story_id": 0,
            "stories": [
                {"@type": "storyInfo", "story_id": 9, "date": 1700000300},
                {"@type": "storyInfo", "story_id": 10, "date": 1700000400}
            ]
        }
    })");

    const QVector<StoryInfo> stories = storiesFromResponse(update);

    QCOMPARE(stories.size(), 2);
    QCOMPARE(stories[0].posterChatId, qint64(42));
    QCOMPARE(stories[0].id, 9);
    QVERIFY(!stories[0].isViewed);
    QCOMPARE(stories[1].id, 10);
}

void StoryRequestsTest::parsesStoriesContainer()
{
    const QJsonObject response = objectFromJson(R"({
        "@type": "stories",
        "total_count": 2,
        "pinned_story_ids": [11],
        "stories": [
            {"@type": "story", "id": 11, "poster_chat_id": -1001234567890,
             "date": 1700000500, "is_posted_to_chat_page": false,
             "content": {"@type": "storyContentPhoto",
                         "photo": {"sizes": [
                             {"width": 320, "height": 480, "photo": {"id": 900}},
                             {"width": 1080, "height": 1920, "photo": {"id": 901}}
                         ]}}},
            {"@type": "story", "id": 12, "poster_chat_id": -1001234567890,
             "date": 1700000600, "is_posted_to_chat_page": false,
             "content": {"@type": "storyContentVideo",
                         "video": {"video": {"id": 777}},
                         "alternative_video": {"video": {"id": 888}}}}
        ]
    })");

    const QVector<StoryInfo> stories = storiesFromResponse(response);

    QCOMPARE(stories.size(), 2);
    QCOMPARE(stories[0].posterChatId, qint64(-1001234567890LL));
    QCOMPARE(stories[0].id, 11);
    QCOMPARE(stories[0].date, qint64(1700000500));
    QVERIFY(stories[0].isPinned);
    QCOMPARE(stories[0].photoFileId, 901);
    QCOMPARE(stories[0].videoFileId, 0);
    QVERIFY(!stories[0].isViewed);

    QCOMPARE(stories[1].id, 12);
    QCOMPARE(stories[1].date, qint64(1700000600));
    QVERIFY(!stories[1].isPinned);
    QCOMPARE(stories[1].photoFileId, 0);
    QCOMPARE(stories[1].videoFileId, 777);
}

void StoryRequestsTest::parsesSingleStory()
{
    const QJsonObject response = objectFromJson(R"({
        "@type": "story",
        "id": 7,
        "poster_chat_id": 42,
        "date": 1700000700,
        "is_posted_to_chat_page": true,
        "content": {"@type": "storyContentPhoto",
                    "photo": {"sizes": [
                        {"width": 100, "height": 200, "photo": {"id": 501}}
                    ]}}
    })");

    const QVector<StoryInfo> stories = storiesFromResponse(response);

    QCOMPARE(stories.size(), 1);
    QCOMPARE(stories.first().posterChatId, qint64(42));
    QCOMPARE(stories.first().id, 7);
    QCOMPARE(stories.first().date, qint64(1700000700));
    // A single story has no pinned_story_ids list, so is_posted_to_chat_page is
    // the schema's pin flag.
    QVERIFY(stories.first().isPinned);
    QCOMPARE(stories.first().photoFileId, 501);
    QCOMPARE(stories.first().videoFileId, 0);
    // Only active-story containers carry the read watermark.
    QVERIFY(!stories.first().isViewed);
}

void StoryRequestsTest::readsPinnedStoryIds()
{
    // An explicit, empty pinned list is authoritative and overrides the story's
    // own is_posted_to_chat_page bit.
    const QJsonObject response = objectFromJson(R"({
        "@type": "stories",
        "total_count": 1,
        "pinned_story_ids": [],
        "stories": [
            {"@type": "story", "id": 3, "poster_chat_id": 42, "date": 1,
             "is_posted_to_chat_page": true,
             "content": {"@type": "storyContentUnsupported"}}
        ]
    })");

    const QVector<StoryInfo> stories = storiesFromResponse(response);

    QCOMPARE(stories.size(), 1);
    QVERIFY(!stories.first().isPinned);
}

void StoryRequestsTest::readsLargestPhotoSize()
{
    const QJsonObject response = objectFromJson(R"({
        "@type": "story",
        "id": 1,
        "poster_chat_id": 42,
        "date": 1,
        "content": {"@type": "storyContentPhoto",
                    "photo": {"sizes": [
                        {"width": 1080, "height": 1920, "photo": {"id": 100}},
                        {"width": 320, "height": 480, "photo": {"id": 101}},
                        {"width": 720, "height": 1280, "photo": {"id": 102}}
                    ]}}
    })");

    const QVector<StoryInfo> stories = storiesFromResponse(response);

    QCOMPARE(stories.size(), 1);
    QCOMPARE(stories.first().photoFileId, 100);
}

void StoryRequestsTest::readsMainVideoFileId()
{
    const QJsonObject response = objectFromJson(R"({
        "@type": "story",
        "id": 2,
        "poster_chat_id": 42,
        "date": 1,
        "content": {"@type": "storyContentVideo",
                    "video": {"video": {"id": 777}},
                    "alternative_video": {"video": {"id": 888}}}
    })");

    const QVector<StoryInfo> stories = storiesFromResponse(response);

    QCOMPARE(stories.size(), 1);
    // The main H.265 rendition is the story's video; the H.264 alternative is a
    // playback fallback and is not reported as the story file.
    QCOMPARE(stories.first().videoFileId, 777);
    QCOMPARE(stories.first().photoFileId, 0);
}

void StoryRequestsTest::handlesUnsupportedAndMissingContent()
{
    const QJsonObject unsupported = objectFromJson(R"({
        "@type": "story",
        "id": 9,
        "poster_chat_id": 42,
        "date": 1,
        "content": {"@type": "storyContentUnsupported"}
    })");
    const QVector<StoryInfo> unsupportedStories = storiesFromResponse(unsupported);
    QCOMPARE(unsupportedStories.size(), 1);
    QCOMPARE(unsupportedStories.first().photoFileId, 0);
    QCOMPARE(unsupportedStories.first().videoFileId, 0);

    const QJsonObject missing = objectFromJson(R"({
        "@type": "story",
        "id": 10,
        "poster_chat_id": 42,
        "date": 1
    })");
    const QVector<StoryInfo> missingStories = storiesFromResponse(missing);
    QCOMPARE(missingStories.size(), 1);
    QCOMPARE(missingStories.first().id, 10);
    QCOMPARE(missingStories.first().photoFileId, 0);
    QCOMPARE(missingStories.first().videoFileId, 0);
}

void StoryRequestsTest::keepsUnreadStoryUnread()
{
    const QJsonObject response = objectFromJson(R"({
        "@type": "chatActiveStories",
        "chat_id": 42,
        "max_read_story_id": 0,
        "stories": [
            {"@type": "storyInfo", "story_id": 1, "date": 1},
            {"@type": "storyInfo", "story_id": 2, "date": 2}
        ]
    })");

    const QVector<StoryInfo> stories = storiesFromResponse(response);

    QCOMPARE(stories.size(), 2);
    QVERIFY(!stories[0].isViewed);
    QVERIFY(!stories[1].isViewed);
}

void StoryRequestsTest::toleratesMissingAndWronglyTypedFields()
{
    QVERIFY(storiesFromResponse(QJsonObject{}).isEmpty());

    const QJsonObject noStories = objectFromJson(R"({
        "@type": "chatActiveStories",
        "chat_id": 42,
        "max_read_story_id": 5
    })");
    QVERIFY(storiesFromResponse(noStories).isEmpty());

    const QJsonObject malformed = objectFromJson(R"({
        "@type": "chatActiveStories",
        "chat_id": "42",
        "max_read_story_id": "5",
        "stories": [
            {"@type": "storyInfo", "story_id": "4", "date": "1700000000"},
            {"@type": "storyInfo", "story_id": 5, "date": true}
        ]
    })");
    const QVector<StoryInfo> stories = storiesFromResponse(malformed);
    QCOMPARE(stories.size(), 2);
    QCOMPARE(stories[0].posterChatId, qint64(42));
    QCOMPARE(stories[0].id, 4);
    QCOMPARE(stories[0].date, qint64(1700000000));
    QVERIFY(stories[0].isViewed);
    QCOMPARE(stories[1].id, 5);
    QCOMPARE(stories[1].date, qint64(0));
    QVERIFY(stories[1].isViewed);
    QVERIFY(!stories[1].isPinned);
}

void StoryRequestsTest::parsesStringEncodedIds()
{
    const QJsonObject response = objectFromJson(R"({
        "@type": "stories",
        "total_count": 1,
        "pinned_story_ids": ["3"],
        "stories": [
            {"@type": "story", "id": "3", "poster_chat_id": "-1001234567890",
             "date": "1700000800",
             "content": {"@type": "storyContentPhoto",
                         "photo": {"sizes": [
                             {"width": 100, "height": 200, "photo": {"id": 600}}
                         ]}}}
        ]
    })");

    const QVector<StoryInfo> stories = storiesFromResponse(response);

    QCOMPARE(stories.size(), 1);
    QCOMPARE(stories.first().id, 3);
    QCOMPARE(stories.first().posterChatId, qint64(-1001234567890LL));
    QCOMPARE(stories.first().date, qint64(1700000800));
    QVERIFY(stories.first().isPinned);
    QCOMPARE(stories.first().photoFileId, 600);
}

void StoryRequestsTest::skipsEntriesWithoutStoryId()
{
    const QJsonObject response = objectFromJson(R"({
        "@type": "chatActiveStories",
        "chat_id": 42,
        "max_read_story_id": 0,
        "stories": [
            42,
            "not a story",
            null,
            {"@type": "storyInfo", "story_id": 0, "date": 1},
            {"@type": "storyInfo", "story_id": -3, "date": 1},
            {"@type": "storyInfo", "date": 1}
        ]
    })");

    QVERIFY(storiesFromResponse(response).isEmpty());
}

void StoryRequestsTest::emptyResponsesYieldNothing()
{
    const QJsonObject emptyStories = objectFromJson(R"({
        "@type": "chatActiveStories",
        "chat_id": 42,
        "max_read_story_id": 0,
        "stories": []
    })");
    QVERIFY(storiesFromResponse(emptyStories).isEmpty());

    const QJsonObject emptyContainer = objectFromJson(R"({
        "@type": "stories",
        "total_count": 0,
        "stories": []
    })");
    QVERIFY(storiesFromResponse(emptyContainer).isEmpty());

    const QJsonObject unrelated = objectFromJson(R"({
        "@type": "ok"
    })");
    QVERIFY(storiesFromResponse(unrelated).isEmpty());
}

void StoryRequestsTest::requestsRoundTripThroughJson()
{
    const QJsonObject active = serialized(getChatActiveStoriesRequest(42));
    QCOMPARE(active.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatActiveStories"));
    QCOMPARE(numberAt(active, QStringLiteral("chat_id")), qint64(42));

    const QJsonObject story = serialized(getStoryRequest(42, 7));
    QCOMPARE(story.value(QStringLiteral("@type")).toString(), QStringLiteral("getStory"));
    QCOMPARE(numberAt(story, QStringLiteral("story_poster_chat_id")), qint64(42));
    QCOMPARE(numberAt(story, QStringLiteral("story_id")), qint64(7));
    QVERIFY(!story.value(QStringLiteral("only_local")).toBool());

    const QJsonObject open = serialized(openStoryRequest(42, 7));
    QCOMPARE(open.value(QStringLiteral("@type")).toString(), QStringLiteral("openStory"));
    QCOMPARE(numberAt(open, QStringLiteral("story_id")), qint64(7));

    const QJsonObject close = serialized(closeStoryRequest(42, 7));
    QCOMPARE(close.value(QStringLiteral("@type")).toString(), QStringLiteral("closeStory"));
    QCOMPARE(numberAt(close, QStringLiteral("story_id")), qint64(7));
}

QTEST_GUILESS_MAIN(StoryRequestsTest)
#include "tst_story_requests.moc"
