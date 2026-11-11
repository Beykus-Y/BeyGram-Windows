#include "MessageSearch.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinSearchLimit = 1;
constexpr int kMaxSearchLimit = 100;

// TDLib rejects limits outside 1..100 for both search functions.
int clampedLimit(int limit)
{
    return std::clamp(limit, kMinSearchLimit, kMaxSearchLimit);
}

void appendMessage(QVector<SearchHit>& hits, const QJsonObject& message)
{
    const QString type = message.value(QStringLiteral("@type")).toString();
    if (!type.isEmpty() && type != QLatin1String("message"))
        return;

    const qint64 messageId = jsonInt64(message.value(QStringLiteral("id")));
    if (messageId <= 0)
        return;

    const MessageData data = messageFromJson(message);

    SearchHit hit;
    hit.chatId = data.chatId;
    hit.messageId = messageId;
    hit.date = data.date;
    hit.chatTitle = message.value(QStringLiteral("chat_title")).toString();
    hit.senderName = message.value(QStringLiteral("sender_name")).toString();
    if (hit.senderName.isEmpty())
        hit.senderName = message.value(QStringLiteral("author_signature")).toString();
    hit.text = data.text;
    hit.textHtml = data.textHtml;
    hits.append(hit);
}

// A response entry is normally a message, but a nested found* container is
// accepted too so a caller can pass a whole sub-response as one entry.
void appendEntry(QVector<SearchHit>& hits, const QJsonValue& value, int depth)
{
    if (depth > 2 || !value.isObject())
        return;

    const QJsonObject entry = value.toObject();
    const QJsonValue nestedValue = entry.value(QStringLiteral("messages"));
    if (nestedValue.isArray()) {
        const QJsonArray nested = nestedValue.toArray();
        for (const QJsonValue& inner : nested)
            appendEntry(hits, inner, depth + 1);
        return;
    }

    appendMessage(hits, entry);
}

} // namespace

QJsonObject searchChatMessagesRequest(qint64 chatId, const QString& query,
                                      qint64 fromMessageId, int limit)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("searchChatMessages")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("query"), query},
        {QStringLiteral("sender_id"), QJsonValue::Null},
        {QStringLiteral("from_message_id"), fromMessageId},
        {QStringLiteral("offset"), 0},
        {QStringLiteral("limit"), clampedLimit(limit)},
        {QStringLiteral("filter"), QJsonValue::Null},
    };
}

QJsonObject searchMessagesRequest(const QString& query, qint64 /*fromMessageId*/, int limit)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("searchMessages")},
        {QStringLiteral("chat_list"), QJsonValue::Null},
        {QStringLiteral("query"), query},
        {QStringLiteral("offset"), QString()},
        {QStringLiteral("limit"), clampedLimit(limit)},
        {QStringLiteral("filter"), QJsonValue::Null},
        {QStringLiteral("chat_type_filter"), QJsonValue::Null},
        {QStringLiteral("min_date"), 0},
        {QStringLiteral("max_date"), 0},
    };
}

QVector<SearchHit> searchHitsFromResponse(const QJsonObject& response)
{
    QVector<SearchHit> hits;

    const QJsonValue messagesValue = response.value(QStringLiteral("messages"));
    if (!messagesValue.isArray())
        return hits;

    const QJsonArray messages = messagesValue.toArray();
    hits.reserve(messages.size());
    for (const QJsonValue& value : messages)
        appendEntry(hits, value, 0);

    return hits;
}

bool searchResponseHasMore(const QJsonObject& response)
{
    const QJsonValue nextMessageId = response.value(QStringLiteral("next_from_message_id"));
    if (!nextMessageId.isUndefined() && !nextMessageId.isNull())
        return jsonInt64(nextMessageId) != 0;

    const QJsonValue nextOffset = response.value(QStringLiteral("next_offset"));
    if (nextOffset.isString())
        return !nextOffset.toString().isEmpty();
    if (nextOffset.isDouble())
        return nextOffset.toInt() != 0;

    return false;
}

} // namespace beygram::telegram
