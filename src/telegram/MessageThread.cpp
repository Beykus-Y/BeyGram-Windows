#include "MessageThread.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <limits>

namespace beygram::telegram {
namespace {

constexpr int kMinHistoryLimit = 1;
constexpr int kMaxHistoryLimit = 100;

bool validId(qint64 id)
{
    return id > 0;
}

int clampedReplyCount(qint64 count)
{
    return static_cast<int>(
        std::clamp<qint64>(count, 0, std::numeric_limits<int>::max()));
}

} // namespace

QJsonObject getMessageThreadRequest(qint64 chatId, qint64 messageId)
{
    if (!validId(chatId) || !validId(messageId))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getMessageThread")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
    };
}

QJsonObject getMessageThreadHistoryRequest(qint64 chatId, qint64 messageId,
                                           qint64 fromMessageId, int offset, int limit)
{
    if (!validId(chatId) || !validId(messageId))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getMessageThreadHistory")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        // 0 is the newest page, not a missing cursor, so a negative value is
        // normalised instead of rejecting the whole request.
        {QStringLiteral("from_message_id"), fromMessageId > 0 ? fromMessageId : 0},
        {QStringLiteral("offset"), std::max(offset, 0)},
        {QStringLiteral("limit"), std::clamp(limit, kMinHistoryLimit, kMaxHistoryLimit)},
    };
}

MessageThreadInfo messageThreadFromResponse(const QJsonObject& response)
{
    MessageThreadInfo info;

    info.chatId = jsonInt64(response.value(QStringLiteral("chat_id")));

    // TDLib names the thread id message_thread_id. A flattened or cached
    // payload may carry the same value as message_id.
    info.messageId = jsonInt64(response.value(QStringLiteral("message_thread_id")));
    if (info.messageId <= 0)
        info.messageId = jsonInt64(response.value(QStringLiteral("message_id")));

    const QJsonObject replyInfo = response.value(QStringLiteral("reply_info")).toObject();
    info.replyCount =
        clampedReplyCount(jsonInt64(replyInfo.value(QStringLiteral("reply_count"))));

    const QJsonValue recentValue = response.value(QStringLiteral("recent_message_ids"));
    if (!recentValue.isArray())
        return info;

    const QJsonArray recent = recentValue.toArray();
    info.recentMessageIds.reserve(recent.size());

    QSet<qint64> seen;
    seen.reserve(recent.size());
    for (const QJsonValue& value : recent) {
        const qint64 id = jsonInt64(value);
        if (id <= 0 || seen.contains(id))
            continue;
        seen.insert(id);
        info.recentMessageIds.append(id);
    }

    return info;
}

} // namespace beygram::telegram
