#include "StoryRequests.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QString>

#include <limits>

namespace beygram::telegram {
namespace {

// TDLib sends real booleans. Anything else (missing, null, a number, a string)
// keeps the fallback instead of silently reading as false.
bool boolField(const QJsonObject& object, const QString& key, bool fallback)
{
    const QJsonValue value = object.value(key);
    return value.isBool() ? value.toBool() : fallback;
}

// Story ids, dates and file ids are int32/int53 on the wire. The JSON interface
// may also carry them as strings, so both spellings are accepted; a value that
// does not fit a positive int yields 0.
int positiveInt(const QJsonValue& value)
{
    const qint64 number = jsonInt64(value);
    if (number <= 0 || number > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(number);
}

int fileIdOf(const QJsonObject& file)
{
    return positiveInt(file.value(QStringLiteral("id")));
}

bool isFullStory(const QJsonObject& story)
{
    if (story.value(QStringLiteral("@type")).toString() == QLatin1String("story"))
        return true;

    // storyInfo has neither content nor a poster chat; both are required by the
    // full story type, so either member identifies one.
    return story.contains(QStringLiteral("content"))
        || story.contains(QStringLiteral("poster_chat_id"));
}

int storyIdOf(const QJsonObject& story)
{
    return positiveInt(story.value(QStringLiteral("id")));
}

int storyInfoIdOf(const QJsonObject& storyInfo)
{
    const int id = positiveInt(storyInfo.value(QStringLiteral("story_id")));
    return id > 0 ? id : storyIdOf(storyInfo);
}

QJsonObject unwrapActiveStories(const QJsonObject& response)
{
    if (response.value(QStringLiteral("@type")).toString()
        != QLatin1String("updateChatActiveStories")) {
        return response;
    }

    const QJsonObject active = response.value(QStringLiteral("active_stories")).toObject();
    return active.isEmpty() ? response : active;
}

QSet<int> pinnedStoryIds(const QJsonObject& container)
{
    QSet<int> ids;
    const QJsonValue value = container.value(QStringLiteral("pinned_story_ids"));
    if (!value.isArray())
        return ids;

    for (const QJsonValue& entry : value.toArray()) {
        const int id = positiveInt(entry);
        if (id > 0)
            ids.insert(id);
    }
    return ids;
}

void applyContent(StoryInfo& info, const QJsonObject& content)
{
    const QString type = content.value(QStringLiteral("@type")).toString();

    if (type == QLatin1String("storyContentPhoto")) {
        info.photoFileId =
            photoLargestFileId(content.value(QStringLiteral("photo")).toObject());
    } else if (type == QLatin1String("storyContentVideo")) {
        // storyVideo.video is the main MPEG4 rendition; alternative_video is the
        // H.264 fallback and is deliberately not chosen here.
        info.videoFileId = fileIdOf(content.value(QStringLiteral("video"))
                                        .toObject()
                                        .value(QStringLiteral("video"))
                                        .toObject());
    }
}

void appendFullStory(QVector<StoryInfo>& result, const QJsonObject& story,
                     bool hasPinnedIds, const QSet<int>& pinnedIds,
                     qint64 maxReadStoryId)
{
    StoryInfo info;
    info.posterChatId = jsonInt64(story.value(QStringLiteral("poster_chat_id")));
    info.id = storyIdOf(story);
    info.date = jsonInt64(story.value(QStringLiteral("date")));
    info.isPinned = hasPinnedIds
        ? pinnedIds.contains(info.id)
        : boolField(story, QStringLiteral("is_posted_to_chat_page"), false);
    info.isViewed = maxReadStoryId > 0 && info.id > 0 && info.id <= maxReadStoryId;
    applyContent(info, story.value(QStringLiteral("content")).toObject());

    if (info.id > 0)
        result.append(info);
}

} // namespace

QJsonObject getChatActiveStoriesRequest(qint64 chatId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatActiveStories")},
        {QStringLiteral("chat_id"), chatId},
    };
}

QJsonObject getStoryRequest(qint64 storyPosterChatId, int storyId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getStory")},
        {QStringLiteral("story_poster_chat_id"), storyPosterChatId},
        {QStringLiteral("story_id"), storyId},
        // The schema has no default for only_local; false asks for the server
        // copy and still returns an already cached story.
        {QStringLiteral("only_local"), false},
    };
}

QJsonObject openStoryRequest(qint64 storyPosterChatId, int storyId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("openStory")},
        {QStringLiteral("story_poster_chat_id"), storyPosterChatId},
        {QStringLiteral("story_id"), storyId},
    };
}

QJsonObject closeStoryRequest(qint64 storyPosterChatId, int storyId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("closeStory")},
        {QStringLiteral("story_poster_chat_id"), storyPosterChatId},
        {QStringLiteral("story_id"), storyId},
    };
}

QVector<StoryInfo> storiesFromResponse(const QJsonObject& response)
{
    QVector<StoryInfo> result;

    const QJsonObject container = unwrapActiveStories(response);
    const QJsonValue storiesValue = container.value(QStringLiteral("stories"));

    if (storiesValue.isArray()) {
        const QJsonArray stories = storiesValue.toArray();
        if (stories.isEmpty())
            return result;

        const QJsonValue pinnedValue = container.value(QStringLiteral("pinned_story_ids"));
        const bool hasPinnedIds = pinnedValue.isArray();
        const QSet<int> pinnedIds = pinnedStoryIds(container);
        const qint64 containerChatId = jsonInt64(container.value(QStringLiteral("chat_id")));
        const qint64 maxReadStoryId =
            jsonInt64(container.value(QStringLiteral("max_read_story_id")));

        for (const QJsonValue& value : stories) {
            if (!value.isObject())
                continue;

            const QJsonObject story = value.toObject();
            if (isFullStory(story)) {
                appendFullStory(result, story, hasPinnedIds, pinnedIds, maxReadStoryId);
                continue;
            }

            StoryInfo info;
            info.posterChatId = containerChatId;
            info.id = storyInfoIdOf(story);
            info.date = jsonInt64(story.value(QStringLiteral("date")));
            info.isPinned = hasPinnedIds && pinnedIds.contains(info.id);
            // max_read_story_id is the last read active story; it is absent
            // from the archived/posted-to-page containers, where every entry
            // therefore reports false.
            info.isViewed =
                maxReadStoryId > 0 && info.id > 0 && info.id <= maxReadStoryId;

            if (info.id > 0)
                result.append(info);
        }
        return result;
    }

    if (isFullStory(response))
        appendFullStory(result, response, false, {}, 0);

    return result;
}

} // namespace beygram::telegram
