#include "TelegramRequests.h"

#include <QJsonArray>

namespace beygram::telegram {
namespace {

QJsonObject inputTextContent(const QString& text, bool clearDraft)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputMessageText")},
        {QStringLiteral("text"),
         QJsonObject{{QStringLiteral("@type"), QStringLiteral("formattedText")},
                     {QStringLiteral("text"), text},
                     {QStringLiteral("entities"), QJsonArray()}}},
        {QStringLiteral("link_preview_options"),
         QJsonObject{{QStringLiteral("@type"), QStringLiteral("linkPreviewOptions")},
                     {QStringLiteral("is_disabled"), false},
                     {QStringLiteral("url"), QString()},
                     {QStringLiteral("force_small_media"), false},
                     {QStringLiteral("force_large_media"), false},
                     {QStringLiteral("show_above_text"), false}}},
        {QStringLiteral("clear_draft"), clearDraft},
    };
}

} // namespace

QJsonObject sendTextMessageRequest(qint64 chatId, const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendMessage")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("reply_to"), QJsonValue::Null},
        {QStringLiteral("options"), QJsonValue::Null},
        {QStringLiteral("reply_markup"), QJsonValue::Null},
        {QStringLiteral("input_message_content"), inputTextContent(text, true)},
    };
}

QJsonObject editTextMessageRequest(qint64 chatId, qint64 messageId, const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("editMessageText")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("reply_markup"), QJsonValue::Null},
        {QStringLiteral("input_message_content"), inputTextContent(text, false)},
    };
}

QJsonObject getChatHistoryRequest(qint64 chatId, qint64 fromMessageId, int limit,
                                  bool onlyLocal)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatHistory")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("from_message_id"), fromMessageId},
        {QStringLiteral("offset"), 0},
        {QStringLiteral("limit"), limit},
        {QStringLiteral("only_local"), onlyLocal},
    };
}

QJsonObject viewMessagesRequest(qint64 chatId, const QJsonArray& messageIds, bool forceRead)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("viewMessages")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_ids"), messageIds},
        {QStringLiteral("source"),
         QJsonObject{{QStringLiteral("@type"), QStringLiteral("messageSourceChatHistory")}}},
        {QStringLiteral("force_read"), forceRead},
    };
}

QJsonObject deleteMessagesRequest(qint64 chatId, const QJsonArray& messageIds, bool revoke)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("deleteMessages")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_ids"), messageIds},
        {QStringLiteral("revoke"), revoke},
    };
}

} // namespace beygram::telegram
