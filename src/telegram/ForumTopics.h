#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One topic of a forum supergroup, as returned by getForumTopics.
///
/// topicId is the value a message must carry to be sent into the topic; TDLib
/// never returns 0 for a real topic, so the parser drops such entries.
struct ForumTopicRow {
    qint64 topicId = 0;
    QString title;
    int unreadCount = 0;
    bool pinned = false;
    bool closed = false;
    qint64 lastMessageDate = 0; // date of last_message, 0 when it is absent
    qint64 iconFileId = 0;      // custom emoji id of the icon, 0 when there is none
};

/// Builds getForumTopics for one page of a forum.
///
/// A non-positive chatId yields {} because TDLib has no chat 0. limit is
/// clamped to 1..100, the page size TDLib serves; negative offsets become 0,
/// which is how TDLib spells "start from the newest topic". offset_date is an
/// int32 in the schema, so larger values are clamped to its maximum. The
/// builder carries no topic cursor, so offset_forum_topic_id is always 0.
[[nodiscard]] QJsonObject getForumTopicsRequest(qint64 chatId, int limit, qint64 offsetDate,
                                                qint64 offsetMessageId);

/// Builds getForumTopic. Returns {} unless both ids are positive.
[[nodiscard]] QJsonObject getForumTopicRequest(qint64 chatId, qint64 topicId);

/// Builds toggleForumTopicIsPinned. Returns {} unless both ids are positive.
[[nodiscard]] QJsonObject toggleForumTopicIsPinnedRequest(qint64 chatId, qint64 topicId,
                                                          bool pinned);

/// Builds toggleForumTopicIsClosed. Returns {} unless both ids are positive.
[[nodiscard]] QJsonObject toggleForumTopicIsClosedRequest(qint64 chatId, qint64 topicId,
                                                          bool closed);

/// Builds createForumTopic. The title is trimmed and must contain 1..128
/// Unicode code points; otherwise {} is returned, matching the Android client,
/// which rejects rather than truncates an invalid name. The icon is the default
/// colour with no custom emoji, so a new topic looks like the Android one.
[[nodiscard]] QJsonObject createForumTopicRequest(qint64 chatId, const QString& title);

/// Builds editForumTopic. The title follows the same trimming and 1..128 code
/// point rule as createForumTopicRequest. The topic icon is left unchanged.
/// Returns {} for a non-positive id or an invalid title.
[[nodiscard]] QJsonObject editForumTopicRequest(qint64 chatId, qint64 topicId,
                                                const QString& title);

/// Parses a forumTopics response into display rows.
///
/// Entries without a positive topic id or without a non-empty title are
/// skipped; the surviving entries keep the server order and are not sorted or
/// deduplicated here. A response of another type, or one without a topics
/// array, yields an empty list.
[[nodiscard]] QVector<ForumTopicRow> forumTopicsFromResponse(const QJsonObject& response);

} // namespace beygram::telegram
