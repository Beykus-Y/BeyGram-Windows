#include "UnreadCounter.h"

#include "TelegramTypes.h"

#include <QJsonValue>

#include <initializer_list>
#include <limits>

namespace beygram::telegram {
namespace {

// Counts arrive as JSON numbers (or numeric strings) and are clamped into the
// int range so a hostile or wrapped payload cannot overflow the totals.
int clampCount(qint64 value)
{
    if (value <= 0)
        return 0;
    constexpr qint64 kMaxCount = std::numeric_limits<int>::max();
    return static_cast<int>(value > kMaxCount ? kMaxCount : value);
}

int saturatedSum(int current, int delta)
{
    const qint64 sum = static_cast<qint64>(current) + static_cast<qint64>(delta);
    return clampCount(sum);
}

bool hasCount(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() || value.isString();
}

// Reads the first of the keys that is actually present. TDLib reports the full
// count as unread_count and the unmuted share as unread_unmuted_count; the
// latter is only a fallback, because it under-reports muted chats.
qint64 firstCount(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const QString name = QString::fromLatin1(key);
        if (hasCount(object, name))
            return jsonInt64(object.value(name));
    }
    return 0;
}

// chat_list is a chatList* object; a bare string is also accepted so callers can
// hand over a list type they already normalized. A missing list is the main one.
QString chatListType(const QJsonValue& value)
{
    if (value.isString())
        return value.toString().trimmed();
    return value.toObject().value(QStringLiteral("@type")).toString();
}

bool isMainChatList(const QString& listType)
{
    return listType.isEmpty() || listType == QLatin1String("chatListMain");
}

} // namespace

UnreadCounts unreadCountsFromUpdate(const QJsonObject& update)
{
    const QString type = update.value(QStringLiteral("@type")).toString();
    const bool chatCountUpdate = type == QLatin1String("updateUnreadChatCount");
    const bool messageCountUpdate = type == QLatin1String("updateUnreadMessageCount");
    if (!chatCountUpdate && !messageCountUpdate)
        return UnreadCounts{};

    // The archive and folder lists report their own totals, which are not part
    // of the main-list aggregate.
    if (!isMainChatList(chatListType(update.value(QStringLiteral("chat_list")))))
        return UnreadCounts{};

    const int count = clampCount(firstCount(update, {"unread_count", "unread_unmuted_count"}));

    UnreadCounts counts;
    if (chatCountUpdate)
        counts.chats = count;
    else
        counts.messages = count;

    counts.mentions = clampCount(firstCount(update, {"mention_count", "unread_mention_count"}));

    const QJsonValue marked = update.value(QStringLiteral("is_marked_as_unread"));
    if (marked.isBool())
        counts.markedUnread = marked.toBool();
    else
        counts.markedUnread = jsonInt64(update.value(QStringLiteral("marked_as_unread_count"))) > 0;
    return counts;
}

UnreadCounts applyUnreadDelta(UnreadCounts current, const QString& listType, int chatDelta,
                              int messageDelta, int mentionDelta)
{
    if (!isMainChatList(listType.trimmed()))
        return current;

    current.chats = saturatedSum(current.chats, chatDelta);
    current.messages = saturatedSum(current.messages, messageDelta);
    current.mentions = saturatedSum(current.mentions, mentionDelta);
    return current;
}

} // namespace beygram::telegram
