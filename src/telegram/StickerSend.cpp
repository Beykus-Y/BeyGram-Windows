#include "StickerSend.h"

#include "ReplyInfo.h"

#include <QJsonValue>
#include <QString>

namespace beygram::telegram {
namespace {

QJsonObject stickerFile(qint64 stickerId, int fileId)
{
    if (fileId > 0) {
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("inputFileId")},
            {QStringLiteral("id"), fileId},
        };
    }
    // Without a registered file id the sticker can only be referenced by a
    // remote identifier; catalog stickers use their sticker id for it.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputFileRemote")},
        {QStringLiteral("id"), QString::number(stickerId)},
    };
}

QJsonObject inputStickerContent(qint64 stickerId, int fileId)
{
    // This TDLib schema wraps the file in inputSticker (sticker, thumbnail,
    // width, height) and keeps emoji on inputMessageSticker itself.
    const QJsonObject sticker{
        {QStringLiteral("@type"), QStringLiteral("inputSticker")},
        {QStringLiteral("sticker"), stickerFile(stickerId, fileId)},
        {QStringLiteral("thumbnail"), QJsonValue::Null},
        {QStringLiteral("width"), 0},
        {QStringLiteral("height"), 0},
    };
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputMessageSticker")},
        {QStringLiteral("sticker"), sticker},
        {QStringLiteral("emoji"), QStringLiteral("")},
    };
}

} // namespace

QJsonObject sendStickerRequest(qint64 chatId, qint64 stickerId, int fileId,
                               qint64 replyToMessageId)
{
    const QJsonValue reply = replyToMessageId > 0
        ? QJsonValue(inputReplyToMessageRequest(replyToMessageId))
        : QJsonValue(QJsonValue::Null);
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendMessage")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("reply_to"), reply},
        {QStringLiteral("options"), QJsonValue::Null},
        {QStringLiteral("reply_markup"), QJsonValue::Null},
        {QStringLiteral("input_message_content"), inputStickerContent(stickerId, fileId)},
    };
}

} // namespace beygram::telegram
