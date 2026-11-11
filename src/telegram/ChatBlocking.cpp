#include "ChatBlocking.h"

#include <QByteArray>
#include <QJsonValue>

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinBlockedSendersLimit = 1;
constexpr int kMaxBlockedSendersLimit = 100;
constexpr int kMinBlockedSendersOffset = 0;

// reportChat.text is limited to 1024 characters by the schema.
constexpr int kMaxReportTextLength = 1024;

// BlockList has only the Main and Stories variants. The schema unblocks with a
// null block_list, so there is no "none" object to build here.
QJsonObject blockListMain()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("blockListMain")},
    };
}

QJsonObject blockRequest(const QJsonObject& sender, bool blocked)
{
    const QJsonValue blockList =
        blocked ? QJsonValue(blockListMain()) : QJsonValue(QJsonValue::Null);
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setMessageSenderBlockList")},
        {QStringLiteral("sender_id"), sender},
        {QStringLiteral("block_list"), blockList},
    };
}

QJsonObject reportChatRequest(qint64 chatId, const QString& optionId,
                              const QJsonArray& messageIds, const QString& text)
{
    // option_id is bytes; the JSON interface carries it base64-encoded, and an
    // empty string means "no option yet", i.e. the initial request.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("reportChat")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("option_id"), optionId},
        {QStringLiteral("message_ids"), messageIds},
        {QStringLiteral("text"), text},
    };
}

// Folds a reason to the bare name of its ReportReason variant: lower case, no
// separators, and without the "reportReason" prefix, so "reportReasonChildAbuse",
// "child abuse" and "child_abuse" all meet on "childabuse".
QString foldedReasonName(const QString& reason)
{
    QString folded;
    folded.reserve(reason.size());
    for (const QChar character : reason) {
        if (character.isLetterOrNumber())
            folded.append(character.toLower());
    }
    const QString prefix = QStringLiteral("reportreason");
    if (folded.startsWith(prefix))
        folded.remove(0, prefix.size());
    return folded;
}

// Keywords that identify a reason inside Telegram's localised report options.
// They are matched against both the decoded option id and the option text, so
// the English and Russian spellings used by Telegram both work.
QStringList reportOptionKeywords(const QString& reason)
{
    const QString type = reportReasonType(reason);
    if (type == QLatin1String("reportReasonSpam"))
        return {QStringLiteral("spam"), QStringLiteral("спам")};
    if (type == QLatin1String("reportReasonViolence"))
        return {QStringLiteral("violence"), QStringLiteral("насили")};
    if (type == QLatin1String("reportReasonPornography"))
        return {QStringLiteral("porn"), QStringLiteral("порн")};
    if (type == QLatin1String("reportReasonChildAbuse"))
        return {QStringLiteral("child"), QStringLiteral("дет")};
    if (type == QLatin1String("reportReasonCopyright"))
        return {QStringLiteral("copyright"), QStringLiteral("авторск")};
    if (type == QLatin1String("reportReasonUnrelatedLocation"))
        return {QStringLiteral("location"), QStringLiteral("мест")};
    if (type == QLatin1String("reportReasonFake"))
        return {QStringLiteral("fake"), QStringLiteral("фейк")};
    if (type == QLatin1String("reportReasonIllegalDrugs"))
        return {QStringLiteral("drug"), QStringLiteral("наркот")};
    if (type == QLatin1String("reportReasonPersonalDetails"))
        return {QStringLiteral("personal"), QStringLiteral("личн")};
    // reportReasonCustom is Telegram's "other" option; its wording is not stable
    // enough to guess, so the caller has to pick it from the offered list.
    return {};
}

} // namespace

QJsonObject messageSenderUser(qint64 userId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSenderUser")},
        {QStringLiteral("user_id"), userId},
    };
}

QJsonObject messageSenderChat(qint64 chatId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSenderChat")},
        {QStringLiteral("chat_id"), chatId},
    };
}

QJsonObject blockSenderRequest(qint64 senderUserId, bool blocked)
{
    return blockRequest(messageSenderUser(senderUserId), blocked);
}

QJsonObject blockSenderRequestChat(qint64 senderChatId, bool blocked)
{
    return blockRequest(messageSenderChat(senderChatId), blocked);
}

QStringList reportReasonTypes()
{
    return {
        QStringLiteral("reportReasonSpam"),
        QStringLiteral("reportReasonViolence"),
        QStringLiteral("reportReasonPornography"),
        QStringLiteral("reportReasonChildAbuse"),
        QStringLiteral("reportReasonCopyright"),
        QStringLiteral("reportReasonUnrelatedLocation"),
        QStringLiteral("reportReasonFake"),
        QStringLiteral("reportReasonIllegalDrugs"),
        QStringLiteral("reportReasonPersonalDetails"),
        QStringLiteral("reportReasonCustom"),
    };
}

QString reportReasonType(const QString& reason)
{
    const QString folded = foldedReasonName(reason);
    if (folded.isEmpty())
        return {};

    for (const QString& type : reportReasonTypes()) {
        if (foldedReasonName(type) == folded)
            return type;
    }
    return {};
}

QJsonObject reportMessageRequest(qint64 chatId, qint64 messageId, const QString& reason)
{
    // A reason this client recognises is chosen from Telegram's option list after
    // the first answer, so it must not become report text. Anything else is
    // free-form detail the user typed, which the schema accepts as text.
    const QString trimmed = reason.trimmed();
    const bool knownReason = !trimmed.isEmpty() && !reportReasonType(trimmed).isEmpty();
    const QString text =
        knownReason ? QString() : trimmed.left(kMaxReportTextLength);

    return reportChatRequest(chatId, QString(), QJsonArray{messageId}, text);
}

QJsonObject reportMessageOptionRequest(qint64 chatId, qint64 messageId,
                                       const QString& optionId, const QString& text)
{
    return reportChatRequest(chatId, optionId, QJsonArray{messageId},
                             text.left(kMaxReportTextLength));
}

QString reportOptionIdForReason(const QJsonArray& options, const QString& reason)
{
    const QStringList keywords = reportOptionKeywords(reason);
    if (keywords.isEmpty())
        return {};

    for (const QJsonValue& value : options) {
        const QJsonObject option = value.toObject();
        // reportOption.id is bytes; the JSON interface base64-encodes it, while
        // Telegram's canonical ids are plain words such as "spam".
        const QString rawId = option.value(QStringLiteral("id")).toString();
        const QString id =
            QString::fromUtf8(QByteArray::fromBase64(rawId.toUtf8()));
        const QString text = option.value(QStringLiteral("text")).toString();

        for (const QString& keyword : keywords) {
            if (id.contains(keyword, Qt::CaseInsensitive)
                || text.contains(keyword, Qt::CaseInsensitive)) {
                return rawId;
            }
        }
    }
    return {};
}

QJsonObject getBlockedSendersRequest(int offset, int limit)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getBlockedMessageSenders")},
        {QStringLiteral("block_list"), blockListMain()},
        {QStringLiteral("offset"), std::max(offset, kMinBlockedSendersOffset)},
        {QStringLiteral("limit"),
         std::clamp(limit, kMinBlockedSendersLimit, kMaxBlockedSendersLimit)},
    };
}

bool responseIsMessageSenders(const QJsonObject& response)
{
    return response.value(QStringLiteral("@type")).toString()
        == QLatin1String("messageSenders");
}

QJsonObject blockSenderFromRepliesRequest(qint64 messageId, bool deleteMessage,
                                          bool deleteAllMessages, bool reportSpam)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("blockMessageSenderFromReplies")},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("delete_message"), deleteMessage},
        {QStringLiteral("delete_all_messages"), deleteAllMessages},
        {QStringLiteral("report_spam"), reportSpam},
    };
}

} // namespace beygram::telegram
