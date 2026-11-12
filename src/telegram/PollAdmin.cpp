#include "PollAdmin.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinPollVotersLimit = 1;
constexpr int kMaxPollVotersLimit = 50;
constexpr int kMinPollVotersOffset = 0;

QJsonObject formattedText(const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("formattedText")},
        {QStringLiteral("text"), text},
        {QStringLiteral("entities"), QJsonArray{}},
    };
}

} // namespace

QJsonObject getPollVotersRequest(qint64 chatId, qint64 messageId, int optionId,
                                 int offset, int limit)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getPollVoters")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("option_id"), optionId},
        {QStringLiteral("offset"), std::max(offset, kMinPollVotersOffset)},
        {QStringLiteral("limit"),
         std::clamp(limit, kMinPollVotersLimit, kMaxPollVotersLimit)},
    };
}

QJsonObject addPollOptionRequest(qint64 chatId, qint64 messageId,
                                 const QString& option)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("addPollOption")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("option"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("inputPollOption")},
             {QStringLiteral("text"), formattedText(option)},
             {QStringLiteral("media"), QJsonValue::Null},
         }},
    };
}

QJsonObject stopPollAdminRequest(qint64 chatId, qint64 messageId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("stopPoll")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("reply_markup"), QJsonValue::Null},
    };
}

QVector<qint64> voterIdsFromResponse(const QJsonObject& response)
{
    QVector<qint64> ids;
    const QJsonArray voters = response.value(QStringLiteral("voters")).toArray();
    ids.reserve(voters.size());

    for (const QJsonValue& value : voters) {
        if (!value.isObject())
            continue;

        const QJsonValue voterId =
            value.toObject().value(QStringLiteral("voter_id"));
        if (!voterId.isObject())
            continue;

        const QJsonObject sender = voterId.toObject();
        const qint64 userId = jsonInt64(sender.value(QStringLiteral("user_id")));
        if (userId > 0) {
            ids.append(userId);
            continue;
        }

        const qint64 chatId = jsonInt64(sender.value(QStringLiteral("chat_id")));
        if (chatId != 0)
            ids.append(chatId);
    }

    return ids;
}

} // namespace beygram::telegram
