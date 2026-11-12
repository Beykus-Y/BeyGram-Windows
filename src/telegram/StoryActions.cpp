#include "StoryActions.h"

#include <QJsonArray>
#include <QJsonValue>

#include <limits>

namespace beygram::telegram {
namespace {

// TDLib types story_id as int32. A larger qint64 cannot be narrowed without
// wrapping, so it names no story and the builders return an empty object.
bool isValidStoryId(qint64 storyId)
{
    return storyId > 0 && storyId <= std::numeric_limits<int>::max();
}

// The poster-aware builders in StoryRequests.h take a poster chat id this
// action layer does not have. They are called with 0 and the placeholder is
// dropped, so the result is the story-id-only part of the request; the caller
// merges the real poster before sending.
QJsonObject withoutPosterChat(QJsonObject request)
{
    request.remove(QStringLiteral("story_poster_chat_id"));
    return request;
}

QJsonObject emojiReactionType(const QString& emoji)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("reactionTypeEmoji")},
        {QStringLiteral("emoji"), emoji},
    };
}

QJsonObject formattedText(const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("formattedText")},
        {QStringLiteral("text"), text},
        {QStringLiteral("entities"), QJsonArray()},
    };
}

// inputMessageReplyToStory names the poster as well as the story. A reply is
// accepted only in the chat that posted the story, so chatId is that poster.
QJsonObject storyReplyTarget(qint64 chatId, qint64 storyId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputMessageReplyToStory")},
        {QStringLiteral("story_poster_chat_id"), chatId},
        {QStringLiteral("story_id"), storyId},
    };
}

QJsonObject replyTextContent(const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputMessageText")},
        {QStringLiteral("text"), formattedText(text)},
        // A null link preview object keeps TDLib's defaults; the viewer does not
        // disable previews for story replies.
        {QStringLiteral("link_preview_options"), QJsonValue::Null},
        // The reply is composed in the story viewer, not in the chat draft.
        {QStringLiteral("clear_draft"), false},
    };
}

} // namespace

QJsonObject openStoryRequest(qint64 storyId)
{
    if (!isValidStoryId(storyId))
        return {};

    return withoutPosterChat(
        beygram::telegram::openStoryRequest(0, static_cast<int>(storyId)));
}

QJsonObject closeStoryRequest(qint64 storyId)
{
    if (!isValidStoryId(storyId))
        return {};

    return withoutPosterChat(
        beygram::telegram::closeStoryRequest(0, static_cast<int>(storyId)));
}

QJsonObject viewStoryRequest(qint64 storyId)
{
    if (!isValidStoryId(storyId))
        return {};

    // The pinned schema has no viewStory: openStory is the viewing call. The
    // name is emitted verbatim for a proxy that exposes it; see the header.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("viewStory")},
        {QStringLiteral("story_id"), storyId},
    };
}

QJsonObject reactStoryRequest(qint64 storyId, const QString& emoji)
{
    if (!isValidStoryId(storyId))
        return {};

    const QString trimmedEmoji = emoji.trimmed();
    const QJsonValue reaction = trimmedEmoji.isEmpty()
        ? QJsonValue(QJsonValue::Null)
        : QJsonValue(emojiReactionType(trimmedEmoji));

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setStoryReaction")},
        {QStringLiteral("story_id"), storyId},
        {QStringLiteral("reaction_type"), reaction},
        {QStringLiteral("update_recent_reactions"), true},
    };
}

QJsonObject replyToStoryRequest(qint64 storyId, qint64 chatId,
                                const QString& text)
{
    if (!isValidStoryId(storyId) || chatId <= 0)
        return {};

    const QString body = text.trimmed();
    if (body.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendMessage")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("reply_to"), storyReplyTarget(chatId, storyId)},
        {QStringLiteral("options"), QJsonValue::Null},
        {QStringLiteral("reply_markup"), QJsonValue::Null},
        {QStringLiteral("input_message_content"), replyTextContent(body)},
    };
}

QJsonObject getStoryRequest(qint64 storyId)
{
    if (!isValidStoryId(storyId))
        return {};

    return withoutPosterChat(
        beygram::telegram::getStoryRequest(0, static_cast<int>(storyId)));
}

} // namespace beygram::telegram
