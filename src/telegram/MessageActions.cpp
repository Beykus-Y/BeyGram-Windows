#include "MessageActions.h"

#include <QString>

#include <algorithm>

namespace beygram::telegram {
namespace {

// Telegram accepts a message TTL of at most one week; 0 disables it.
constexpr int kMaxMessageTtlSeconds = 604800;

bool hasChatId(qint64 chatId)
{
    // Zero names no Telegram chat. Negative ids are groups and channels, so
    // only zero is rejected.
    return chatId != 0;
}

int clampedMessageTtl(int ttlSeconds)
{
    return std::clamp(ttlSeconds, 0, kMaxMessageTtlSeconds);
}

} // namespace

QJsonObject messageActionsPinRequest(qint64 chatId, qint64 messageId, bool pin)
{
    if (!hasChatId(chatId) || messageId <= 0)
        return {};

    if (!pin) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("unpinChatMessage")},
            {QStringLiteral("chat_id"), chatId},
            {QStringLiteral("message_id"), messageId},
        };
    }

    // TDLib requires both flags even though this signature has no way to set
    // them: a partial pinChatMessage is not parsable.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("pinChatMessage")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("disable_notification"), false},
        {QStringLiteral("only_for_self"), false},
    };
}

QJsonObject unpinAllChatMessagesRequest(qint64 chatId)
{
    if (!hasChatId(chatId))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("unpinAllChatMessages")},
        {QStringLiteral("chat_id"), chatId},
    };
}

QJsonObject clearChatHistoryRequest(qint64 chatId, bool removeFromServer)
{
    if (!hasChatId(chatId))
        return {};

    // There is no clearChatHistory method in TDLib: keeping the chat while
    // dropping its messages is deleteChatHistory with remove_from_chat_list
    // false, and revoke carries "also for other participants".
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("deleteChatHistory")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("remove_from_chat_list"), false},
        {QStringLiteral("revoke"), removeFromServer},
    };
}

QJsonObject deleteChatHistoryRequest(qint64 chatId, bool revoke)
{
    if (!hasChatId(chatId))
        return {};

    // Deleting the chat also removes it from every chat list.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("deleteChatHistory")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("remove_from_chat_list"), true},
        {QStringLiteral("revoke"), revoke},
    };
}

QJsonObject messageActionsMarkUnreadRequest(qint64 chatId, bool unread)
{
    if (!hasChatId(chatId))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("toggleChatIsMarkedAsUnread")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("is_marked_as_unread"), unread},
    };
}

QJsonObject setChatMessageTtlRequest(qint64 chatId, int ttlSeconds)
{
    if (!hasChatId(chatId))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setChatMessageTtl")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("ttl"), clampedMessageTtl(ttlSeconds)},
    };
}

QJsonObject messageActionsPinnedMessageRequest(qint64 chatId)
{
    if (!hasChatId(chatId))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatPinnedMessage")},
        {QStringLiteral("chat_id"), chatId},
    };
}

} // namespace beygram::telegram
