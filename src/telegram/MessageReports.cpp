#include "MessageReports.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

#include <algorithm>

namespace beygram::telegram {
namespace {

// reportChat.message_ids carries at most 100 identifiers.
constexpr int kMaxReportMessageIds = 100;

// reportChat.text and reportChatPhoto.text are limited to 1024 characters by
// the schema.
constexpr int kMaxReportCommentLength = 1024;

QJsonArray normalizedMessageIds(const QVector<qint64>& messageIds)
{
    QJsonArray ids;
    QSet<qint64> seen;
    seen.reserve(std::min<qsizetype>(messageIds.size(), kMaxReportMessageIds));

    for (const qint64 id : messageIds) {
        if (id <= 0)
            continue;
        if (seen.contains(id))
            continue;

        seen.insert(id);
        ids.append(id);
        if (ids.size() >= kMaxReportMessageIds)
            break;
    }

    return ids;
}

QString clampedComment(const QString& comment)
{
    return comment.trimmed().left(kMaxReportCommentLength);
}

QJsonArray optionArray(const QJsonObject& response)
{
    const QJsonValue options = response.value(QStringLiteral("options"));
    if (options.isArray())
        return options.toArray();

    const QJsonValue reasons = response.value(QStringLiteral("reasons"));
    if (reasons.isArray())
        return reasons.toArray();

    // Accept a response that wraps the option list in a result object, so a
    // caller that did not unwrap it still gets the reasons.
    const QJsonValue result = response.value(QStringLiteral("result"));
    if (result.isObject())
        return optionArray(result.toObject());

    return {};
}

QString firstString(const QJsonObject& object, const QString& first, const QString& second)
{
    const QString value = object.value(first).toString().trimmed();
    return value.isEmpty() ? object.value(second).toString().trimmed() : value;
}

} // namespace

QVector<ReportReason> reportReasonsFromOptions(const QJsonObject& response)
{
    const QJsonArray options = optionArray(response);

    QVector<ReportReason> reasons;
    reasons.reserve(options.size());

    for (const QJsonValue& value : options) {
        if (!value.isObject())
            continue;

        const QJsonObject option = value.toObject();

        ReportReason reason;
        reason.id = firstString(option, QStringLiteral("id"), QStringLiteral("reason_id"));
        reason.title = firstString(option, QStringLiteral("title"), QStringLiteral("text"));
        if (reason.title.isEmpty())
            reason.title = option.value(QStringLiteral("label")).toString().trimmed();

        // An option without both identifiers cannot be selected and would be a
        // dead row in the report dialog.
        if (reason.id.isEmpty() || reason.title.isEmpty())
            continue;

        reason.description =
            firstString(option, QStringLiteral("description"), QStringLiteral("subtitle"));
        reason.requiresComment =
            option.value(QStringLiteral("requires_comment")).toBool()
            || option.value(QStringLiteral("requiresComment")).toBool();

        reasons.append(reason);
    }

    return reasons;
}

QJsonObject getReportChatOptionsRequest(qint64 chatId,
                                        const QVector<qint64>& messageIds)
{
    if (chatId <= 0)
        return {};

    // This schema has no getReportChatOptions: the first reportChat with an
    // empty option_id is what makes Telegram return the option list.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("reportChat")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("option_id"), QString()},
        {QStringLiteral("message_ids"), normalizedMessageIds(messageIds)},
        {QStringLiteral("text"), QString()},
    };
}

QJsonObject reportChatRequest(qint64 chatId,
                              const QVector<qint64>& messageIds,
                              const QString& reasonId,
                              const QString& comment)
{
    const QString optionId = reasonId.trimmed();
    if (chatId <= 0 || optionId.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("reportChat")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("option_id"), optionId},
        {QStringLiteral("message_ids"), normalizedMessageIds(messageIds)},
        {QStringLiteral("text"), clampedComment(comment)},
    };
}

QJsonObject reportChatPhotoRequest(qint64 chatId, qint64 fileId,
                                   const QString& reasonId,
                                   const QString& comment)
{
    const QString reasonType = reasonId.trimmed();
    if (chatId <= 0 || fileId <= 0 || reasonType.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("reportChatPhoto")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("file_id"), fileId},
        {QStringLiteral("reason"), QJsonObject{
             {QStringLiteral("@type"), reasonType},
         }},
        {QStringLiteral("text"), clampedComment(comment)},
    };
}

QJsonObject reportSupergroupSpamRequest(qint64 supergroupId,
                                        const QVector<qint64>& messageIds)
{
    if (supergroupId <= 0)
        return {};

    const QJsonArray ids = normalizedMessageIds(messageIds);
    if (ids.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("reportSupergroupSpam")},
        {QStringLiteral("supergroup_id"), supergroupId},
        {QStringLiteral("message_ids"), ids},
    };
}

QJsonObject reportSupergroupAntiSpamFalsePositiveRequest(qint64 supergroupId,
                                                         qint64 messageId)
{
    if (supergroupId <= 0 || messageId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("reportSupergroupAntiSpamFalsePositive")},
        {QStringLiteral("supergroup_id"), supergroupId},
        {QStringLiteral("message_id"), messageId},
    };
}

} // namespace beygram::telegram
