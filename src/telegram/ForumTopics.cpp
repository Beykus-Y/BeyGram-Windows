#include "ForumTopics.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <limits>

namespace beygram::telegram {
namespace {

constexpr int kMinTopicsLimit = 1;
constexpr int kMaxTopicsLimit = 100;

// Telegram counts topic names in code points, so a surrogate pair is one
// character even though QString stores it as two UTF-16 units.
constexpr int kMaxTitleLength = 128;

// The colour the Android client gives a new topic. TDLib has no server-side
// default, so the icon object is always written explicitly.
constexpr int kDefaultTopicIconColor = 0x6FB9F0;

constexpr qint64 kMaxOffsetDate = std::numeric_limits<int>::max();

bool validId(qint64 id)
{
    return id > 0;
}

qint64 nonNegative(qint64 value)
{
    return value > 0 ? value : 0;
}

/// Returns the trimmed title, or an empty string when the name is blank or
/// longer than Telegram accepts. Callers treat an empty result as invalid.
QString normalizedTitle(const QString& title)
{
    const QString trimmed = title.trimmed();
    if (trimmed.isEmpty())
        return {};
    if (trimmed.toUcs4().size() > kMaxTitleLength)
        return {};
    return trimmed;
}

bool flagFrom(const QJsonObject& object, const QString& key, bool fallback)
{
    const QJsonValue value = object.value(key);
    return value.isBool() ? value.toBool() : fallback;
}

int unreadCountFrom(const QJsonObject& topic)
{
    const qint64 count = jsonInt64(topic.value(QStringLiteral("unread_count")));
    return static_cast<int>(std::clamp<qint64>(count, 0, std::numeric_limits<int>::max()));
}

} // namespace

QJsonObject getForumTopicsRequest(qint64 chatId, int limit, qint64 offsetDate,
                                  qint64 offsetMessageId)
{
    if (!validId(chatId))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getForumTopics")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("query"), QString()},
        {QStringLiteral("offset_date"), std::clamp<qint64>(offsetDate, 0, kMaxOffsetDate)},
        {QStringLiteral("offset_message_id"), nonNegative(offsetMessageId)},
        // The signature pages by date and message only, so the topic cursor is
        // always the first page's 0; the schema revision still requires it.
        {QStringLiteral("offset_forum_topic_id"), 0},
        {QStringLiteral("limit"), std::clamp(limit, kMinTopicsLimit, kMaxTopicsLimit)},
    };
}

QJsonObject getForumTopicRequest(qint64 chatId, qint64 topicId)
{
    if (!validId(chatId) || !validId(topicId))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getForumTopic")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("forum_topic_id"), topicId},
    };
}

QJsonObject toggleForumTopicIsPinnedRequest(qint64 chatId, qint64 topicId, bool pinned)
{
    if (!validId(chatId) || !validId(topicId))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("toggleForumTopicIsPinned")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("forum_topic_id"), topicId},
        {QStringLiteral("is_pinned"), pinned},
    };
}

QJsonObject toggleForumTopicIsClosedRequest(qint64 chatId, qint64 topicId, bool closed)
{
    if (!validId(chatId) || !validId(topicId))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("toggleForumTopicIsClosed")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("forum_topic_id"), topicId},
        {QStringLiteral("is_closed"), closed},
    };
}

QJsonObject createForumTopicRequest(qint64 chatId, const QString& title)
{
    if (!validId(chatId))
        return {};

    const QString name = normalizedTitle(title);
    if (name.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("createForumTopic")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("name"), name},
        {QStringLiteral("is_name_implicit"), false},
        {QStringLiteral("icon"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("forumTopicIcon")},
             {QStringLiteral("color"), kDefaultTopicIconColor},
             {QStringLiteral("custom_emoji_id"), 0},
         }},
    };
}

QJsonObject editForumTopicRequest(qint64 chatId, qint64 topicId, const QString& title)
{
    if (!validId(chatId) || !validId(topicId))
        return {};

    const QString name = normalizedTitle(title);
    if (name.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("editForumTopic")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("forum_topic_id"), topicId},
        {QStringLiteral("name"), name},
        // The title edit never touches the icon; both icon fields are still
        // written because the schema requires them.
        {QStringLiteral("edit_icon_custom_emoji"), false},
        {QStringLiteral("icon_custom_emoji_id"), 0},
    };
}

QVector<ForumTopicRow> forumTopicsFromResponse(const QJsonObject& response)
{
    QVector<ForumTopicRow> rows;
    if (response.value(QStringLiteral("@type")).toString() != QLatin1String("forumTopics"))
        return rows;

    const QJsonValue topicsValue = response.value(QStringLiteral("topics"));
    if (!topicsValue.isArray())
        return rows;

    const QJsonArray topics = topicsValue.toArray();
    rows.reserve(topics.size());

    for (const QJsonValue& value : topics) {
        if (!value.isObject())
            continue;

        const QJsonObject topic = value.toObject();
        const QJsonObject info = topic.value(QStringLiteral("info")).toObject();

        ForumTopicRow row;

        // forumTopicInfo owns the id; a flattened or cached payload may carry
        // it on the topic itself.
        row.topicId = jsonInt64(info.value(QStringLiteral("forum_topic_id")));
        if (row.topicId <= 0)
            row.topicId = jsonInt64(topic.value(QStringLiteral("forum_topic_id")));
        if (row.topicId <= 0)
            row.topicId = jsonInt64(topic.value(QStringLiteral("topic_id")));
        if (row.topicId <= 0)
            continue;

        row.title = info.value(QStringLiteral("name")).toString();
        if (row.title.trimmed().isEmpty())
            row.title = topic.value(QStringLiteral("title")).toString();
        if (row.title.trimmed().isEmpty())
            row.title = topic.value(QStringLiteral("name")).toString();
        // A row without a title cannot be shown or renamed, so it is dropped
        // instead of surfacing a blank list entry.
        if (row.title.trimmed().isEmpty())
            continue;

        row.unreadCount = unreadCountFrom(topic);

        // is_pinned lives on forumTopic; is_closed lives on forumTopicInfo in
        // the schema pinned by this client. Both are accepted on either object
        // so an older flattened payload still parses.
        row.pinned = flagFrom(topic, QStringLiteral("is_pinned"),
                              flagFrom(info, QStringLiteral("is_pinned"), false));
        row.closed = flagFrom(info, QStringLiteral("is_closed"),
                              flagFrom(topic, QStringLiteral("is_closed"), false));

        const QJsonObject lastMessage =
            topic.value(QStringLiteral("last_message")).toObject();
        row.lastMessageDate =
            nonNegative(jsonInt64(lastMessage.value(QStringLiteral("date"))));

        // TDLib identifies a topic icon by its custom emoji id, not by a file.
        const QJsonObject icon = info.value(QStringLiteral("icon")).toObject();
        row.iconFileId = jsonInt64(icon.value(QStringLiteral("custom_emoji_id")));
        if (row.iconFileId == 0)
            row.iconFileId = jsonInt64(info.value(QStringLiteral("icon_custom_emoji_id")));
        if (row.iconFileId == 0)
            row.iconFileId = jsonInt64(topic.value(QStringLiteral("icon_custom_emoji_id")));

        rows.append(row);
    }

    return rows;
}

} // namespace beygram::telegram
