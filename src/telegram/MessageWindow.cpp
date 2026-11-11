#include "MessageWindow.h"

#include "TelegramTypes.h"

#include <QString>

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinWindowLimit = 1;
constexpr int kMaxWindowLimit = 100;

// getChatHistory accepts offsets down to -99 only.
constexpr int kMaxHistoryRadius = 99;

// TDLib rejects limits outside 1..100.
int clampedLimit(int limit)
{
    return std::clamp(limit, kMinWindowLimit, kMaxWindowLimit);
}

// The offset is negative, and TDLib requires -offset <= limit.
int clampedRadius(int radius, int limit)
{
    return std::clamp(radius, 0, std::min(kMaxHistoryRadius, limit));
}

} // namespace

MessageWindowRequest windowAroundMessageRequest(qint64 chatId, qint64 messageId, int radius,
                                                int limit)
{
    const int windowLimit = clampedLimit(limit);
    const int windowRadius = clampedRadius(radius, windowLimit);

    MessageWindowRequest request;
    request.history = QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatHistory")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("from_message_id"), messageId},
        {QStringLiteral("offset"), -windowRadius},
        {QStringLiteral("limit"), windowLimit},
        {QStringLiteral("only_local"), false},
    };
    request.message = QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getMessage")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
    };
    return request;
}

qint64 messageIdFromResponse(const QJsonObject& response)
{
    return jsonInt64(response.value(QStringLiteral("id")));
}

} // namespace beygram::telegram
