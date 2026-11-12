#include "SearchFilters.h"

#include <QJsonValue>

#include <algorithm>
#include <limits>
#include <utility>

namespace beygram::telegram {
namespace {

constexpr int kMinSearchLimit = 1;
constexpr int kMaxSearchLimit = 100;

// TDLib declares min_date/max_date as int32; anything outside that range is
// rejected by the server rather than clamped by it.
constexpr qint64 kMaxSearchDate = std::numeric_limits<qint32>::max();

// TDLib rejects limits outside 1..100 for both search functions.
int clampedLimit(int limit)
{
    return std::clamp(limit, kMinSearchLimit, kMaxSearchLimit);
}

qint64 clampedDate(qint64 date)
{
    return std::clamp<qint64>(date, 0, kMaxSearchDate);
}

/// Maps the UI kind to TDLib's searchMessagesFilter* union. "any", an empty kind
/// and unknown kinds all become null, which lets the server search every type.
QJsonValue searchFilterForKind(const QString& kind)
{
    const QString normalized = kind.trimmed().toLower();

    if (normalized == QLatin1String("photo")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("searchMessagesFilterPhoto")},
        };
    }
    if (normalized == QLatin1String("video")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("searchMessagesFilterVideo")},
        };
    }
    if (normalized == QLatin1String("document")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("searchMessagesFilterDocument")},
        };
    }
    if (normalized == QLatin1String("audio")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("searchMessagesFilterAudio")},
        };
    }
    if (normalized == QLatin1String("voice")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("searchMessagesFilterVoiceNote")},
        };
    }
    if (normalized == QLatin1String("url")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("searchMessagesFilterUrl")},
        };
    }
    if (normalized == QLatin1String("mention")) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("searchMessagesFilterMention")},
        };
    }

    return QJsonValue::Null;
}

QJsonValue senderForUser(qint64 userId)
{
    if (userId <= 0)
        return QJsonValue::Null;
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSenderUser")},
        {QStringLiteral("user_id"), userId},
    };
}

} // namespace

QJsonObject searchChatMessagesFilteredRequest(qint64 chatId, const QString& query,
                                              const SearchFilter& filter, qint64 fromMessageId,
                                              int limit)
{
    // minDate/maxDate are deliberately absent: searchChatMessages has no date
    // range, so the caller filters the returned page by date instead.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("searchChatMessages")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("query"), query},
        {QStringLiteral("sender_id"), senderForUser(filter.senderUserId)},
        {QStringLiteral("from_message_id"), fromMessageId},
        {QStringLiteral("offset"), 0},
        {QStringLiteral("limit"), clampedLimit(limit)},
        {QStringLiteral("filter"), searchFilterForKind(filter.kind)},
    };
}

QJsonObject searchMessagesFilteredRequest(const QString& query, const SearchFilter& filter,
                                          const QString& offset, int limit)
{
    qint64 minDate = clampedDate(filter.minDate);
    qint64 maxDate = clampedDate(filter.maxDate);
    if (minDate > 0 && maxDate > 0 && minDate > maxDate)
        std::swap(minDate, maxDate);

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("searchMessages")},
        {QStringLiteral("chat_list"), QJsonValue::Null},
        {QStringLiteral("query"), query},
        {QStringLiteral("offset"), offset},
        {QStringLiteral("limit"), clampedLimit(limit)},
        {QStringLiteral("filter"), searchFilterForKind(filter.kind)},
        {QStringLiteral("chat_type_filter"), QJsonValue::Null},
        {QStringLiteral("min_date"), minDate},
        {QStringLiteral("max_date"), maxDate},
    };
}

} // namespace beygram::telegram
