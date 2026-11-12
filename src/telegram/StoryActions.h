#pragma once

#include "StoryRequests.h"

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for the story actions the viewer issues around one story:
/// opening and closing it (read marking), viewing it, reacting, replying and
/// fetching its detail. They contain no logging and no @extra; the transport
/// adds correlation.
///
/// Shape follows the pinned TDLib schema (1.8.69, commit 4c6377aa, see
/// docs/WINDOWS_TDLIB.md):
///
///   openStory story_poster_chat_id:int53 story_id:int32 = Ok;
///   closeStory story_poster_chat_id:int53 story_id:int32 = Ok;
///   getStory story_poster_chat_id:int53 story_id:int32 only_local:Bool = Story;
///   setStoryReaction story_poster_chat_id:int53 story_id:int32
///       reaction_type:ReactionType update_recent_reactions:Bool = Ok;
///   sendMessage chat_id:int53 ... reply_to:InputMessageReplyTo
///       input_message_content:InputMessageContent = Message;
///   inputMessageReplyToStory story_poster_chat_id:int53 story_id:int32
///       = InputMessageReplyTo;
///
/// TDLib addresses a story by the pair (story_poster_chat_id, story_id), not by
/// story_id alone. The action signatures below carry only the story id (and,
/// for a reply, the destination chat), so the poster is not always expressible.
/// Where a StoryRequests.h builder already covers the method, the overload here
/// delegates to it and drops the poster field it cannot fill; where no builder
/// exists, the payload carries the story id and leaves story_poster_chat_id for
/// the caller that owns the story detail. replyToStoryRequest is the exception:
/// its chatId is the poster chat, because Telegram only accepts a story reply in
/// the chat that posted the story.
///
/// viewStoryRequest is not a TDLib function in the pinned schema; viewing a
/// story is openStory, which raises the read watermark. The builder is kept as
/// a verbatim probe for a transport or proxy that exposes the name, and the
/// caller must fall back to openStoryRequest against a pinned server.
///
/// Every builder rejects a non-positive id, and a story id above int32 (the
/// schema's story_id type), with an empty object that the caller must not send.

/// Builds the story-id-only openStory request.
///
/// openStory raises the server read watermark, so it belongs after the story
/// detail and its media are on screen, not when the viewer merely opens. The
/// poster-aware builder is openStoryRequest(storyPosterChatId, storyId) in
/// StoryRequests.h; this overload delegates to it with poster chat id 0 and
/// removes that placeholder again, because the signature carries no poster. The
/// caller must add the real story_poster_chat_id before sending, or call the
/// StoryRequests.h overload directly when the poster is known.
///
/// A story id above int32 is rejected: the schema types story_id as int32 and a
/// narrowing cast would wrap it.
[[nodiscard]] QJsonObject openStoryRequest(qint64 storyId);

/// Builds the story-id-only closeStory request.
///
/// Only meaningful for a story whose openStory was actually sent. The same
/// poster-chat limitation and int32 range check as openStoryRequest apply; the
/// delegated closeStoryRequest(storyPosterChatId, storyId) lives in
/// StoryRequests.h.
[[nodiscard]] QJsonObject closeStoryRequest(qint64 storyId);

/// Builds the story-id-only viewStory request.
///
/// The pinned schema has no viewStory function: openStory is the call that
/// informs TDLib a story is being viewed and raises its read watermark. This
/// builder emits viewStory verbatim for a transport or proxy that exposes the
/// name; a pinned server answers it with 400 "Method not found". Use
/// openStoryRequest(storyId) against the pinned server.
///
/// The payload carries only the story id, so it shares the poster-chat
/// limitation of the other one-argument builders.
[[nodiscard]] QJsonObject viewStoryRequest(qint64 storyId);

/// Builds setStoryReaction for one emoji, or its removal.
///
/// emoji is trimmed. A non-empty emoji is sent as reactionTypeEmoji; an empty
/// (or whitespace-only) emoji sends a null reaction_type, which is how TDLib
/// removes the current reaction. update_recent_reactions is true, matching the
/// message-reaction builder, so the emoji joins the recent list.
///
/// The pinned schema also requires story_poster_chat_id, which this signature
/// cannot express. The payload carries the story id and leaves the poster for
/// the caller that owns the story detail; a pinned server rejects the request
/// until that field is added. Only emoji reactions are expressible: custom
/// emoji and paid reactions need a different signature.
[[nodiscard]] QJsonObject reactStoryRequest(qint64 storyId, const QString& emoji);

/// Builds sendMessage with inputMessageReplyToStory.
///
/// chatId is the chat that receives the reply and must be the private chat that
/// posted the story: TDLib allows story replies only in the poster chat, and
/// channel stories cannot be replied to. The reply target therefore uses chatId
/// as story_poster_chat_id as well as sendMessage.chat_id.
///
/// text is trimmed; an empty body, a non-positive chatId or an invalid story id
/// returns an empty object, which the caller must not send. The content is a
/// plain inputMessageText with no entities and clear_draft false, because the
/// reply is composed in the viewer, not in the chat's draft. options is null
/// (send immediately), topic_id is null (not a forum topic) and reply_markup is
/// null.
[[nodiscard]] QJsonObject replyToStoryRequest(qint64 storyId, qint64 chatId,
                                              const QString& text);

/// Builds the story-id-only getStory request.
///
/// only_local is false, the schema-required flag that asks for the server copy
/// and still returns an already cached story. The poster-aware
/// getStoryRequest(storyPosterChatId, storyId) lives in StoryRequests.h; this
/// overload delegates to it with poster chat id 0 and removes that placeholder
/// again, because the signature carries no poster. The caller must add the real
/// story_poster_chat_id before sending, or call the StoryRequests.h overload
/// directly when the poster is known.
[[nodiscard]] QJsonObject getStoryRequest(qint64 storyId);

} // namespace beygram::telegram
