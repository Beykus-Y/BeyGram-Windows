#pragma once

#include <QJsonObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Basic information about one story, flattened out of the TDLib objects the
/// story bar and viewer read.
///
/// Shape follows the TDLib schema pinned by this client (1.8.69, commit
/// 4c6377aa, see docs/WINDOWS_TDLIB.md):
///
///   getChatActiveStories chat_id:int53 = ChatActiveStories;
///   chatActiveStories chat_id:int53 list:StoryList order:int53
///       can_be_archived:Bool max_read_story_id:int32
///       stories:vector<storyInfo> = ChatActiveStories;
///   storyInfo story_id:int32 date:int32 is_for_close_friends:Bool
///       is_live:Bool = StoryInfo;
///
///   getStory story_poster_chat_id:int53 story_id:int32 only_local:Bool = Story;
///   story id:int32 poster_chat_id:int53 poster_id:MessageSender date:int32
///       ... is_posted_to_chat_page:Bool ... content:StoryContent ... = Story;
///   storyContentPhoto photo:photo = StoryContent;
///   storyContentVideo video:storyVideo alternative_video:storyVideo = StoryContent;
///   storyVideo ... video:file = StoryVideo;
///   photo has_stickers:Bool minithumbnail:minithumbnail sizes:vector<photoSize>
///       = Photo;
///   photoSize type:string photo:file width:int32 height:int32 ... = PhotoSize;
///
///   stories total_count:int32 stories:vector<story>
///       pinned_story_ids:vector<int32> = Stories;
///
///   openStory story_poster_chat_id:int53 story_id:int32 = Ok;
///   closeStory story_poster_chat_id:int53 story_id:int32 = Ok;
struct StoryInfo {
    /// Chat that posted the story. storyInfo carries no poster of its own, so
    /// this is chatActiveStories.chat_id for basic entries.
    qint64 posterChatId = 0;
    /// storyInfo.story_id or story.id; 0 when the payload has no usable id.
    int id = 0;
    /// Point in time (Unix timestamp) when the story was posted.
    qint64 date = 0;
    /// True when the payload marks the story pinned. This revision has no
    /// per-story is_pinned field: a container's pinned_story_ids is
    /// authoritative when present (an empty list means nothing is pinned), and
    /// story.is_posted_to_chat_page (the schema's "shown on the chat's profile
    /// page" flag that pinned_story_ids describes) is used for a full story
    /// object that arrives without a pinned_story_ids list. Basic storyInfo
    /// entries have no pin flag of their own.
    bool isPinned = false;
    /// True when the story id is at or below the container's
    /// max_read_story_id. Only active-story payloads carry that watermark, so
    /// a full story object outside chatActiveStories reports false.
    bool isViewed = false;
    /// File id of the largest photoSize for storyContentPhoto; 0 otherwise.
    int photoFileId = 0;
    /// File id of storyContentVideo.video.video; 0 otherwise. The alternative
    /// H.264 rendition is not used.
    int videoFileId = 0;
};

/// Builds getChatActiveStories chat_id:int53 = ChatActiveStories.
///
/// The answer is a chatActiveStories object; pass it to storiesFromResponse.
[[nodiscard]] QJsonObject getChatActiveStoriesRequest(qint64 chatId);

/// Builds getStory story_poster_chat_id:int53 story_id:int32 only_local:Bool.
///
/// only_local is part of the schema and is always written as false: the viewer
/// needs the server copy, including media that is not cached yet. A locally
/// cached story is still returned by the same request.
[[nodiscard]] QJsonObject getStoryRequest(qint64 storyPosterChatId, int storyId);

/// Builds openStory story_poster_chat_id:int53 story_id:int32 = Ok.
///
/// Sending this raises the server read watermark, so it belongs after the story
/// detail and its media are on screen, not when the viewer merely opens.
[[nodiscard]] QJsonObject openStoryRequest(qint64 storyPosterChatId, int storyId);

/// Builds closeStory story_poster_chat_id:int53 story_id:int32 = Ok.
///
/// Only meaningful for a story whose openStory was actually sent.
[[nodiscard]] QJsonObject closeStoryRequest(qint64 storyPosterChatId, int storyId);

/// Flattens the story objects carried by a TDLib response.
///
/// Accepted shapes:
///   - chatActiveStories, the getChatActiveStories answer;
///   - updateChatActiveStories, whose active_stories member is unwrapped;
///   - stories, the getChatArchivedStories / getChatPostedToChatPageStories
///     answer, with pinned_story_ids applied to its full story entries;
///   - a single story object, the getStory / postStory answer.
///
/// Basic storyInfo entries fill posterChatId from the container's chat_id and
/// leave the media ids 0; full story entries fill posterChatId from
/// poster_chat_id and read the photo or video file id from content.
///
/// Entries without an object shape or with a non-positive id are skipped.
/// Missing or wrongly typed fields keep the defaults above. This is parsing
/// only: live, secret and blocked stories are not filtered here, because those
/// rules belong to the caller that owns the chat and block-list state.
[[nodiscard]] QVector<StoryInfo> storiesFromResponse(const QJsonObject& response);

} // namespace beygram::telegram
