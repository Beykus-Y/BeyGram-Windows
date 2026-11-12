#include "AlbumSend.h"

#include "ReplyInfo.h"

#include <QJsonArray>
#include <QJsonValue>

namespace beygram::telegram {
namespace {

// sendMessageAlbum accepts at most ten messages; see the TDLib schema note on
// input_message_contents.
constexpr int kMaxAlbumItems = 10;

QJsonObject formattedCaption(const QString& caption)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("formattedText")},
        {QStringLiteral("text"), caption},
        {QStringLiteral("entities"), QJsonArray()},
    };
}

QJsonObject localFile(const QString& localPath)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputFileLocal")},
        {QStringLiteral("path"), localPath},
    };
}

int mediaDimension(int value)
{
    return value > 0 ? value : 0;
}

QJsonObject inputPhotoContent(const AlbumItem& item, const QString& caption)
{
    const QJsonObject photo{
        {QStringLiteral("@type"), QStringLiteral("inputPhoto")},
        {QStringLiteral("photo"), localFile(item.path)},
        {QStringLiteral("thumbnail"), QJsonValue::Null},
        {QStringLiteral("video"), QJsonValue::Null},
        {QStringLiteral("added_sticker_file_ids"), QJsonArray()},
        {QStringLiteral("width"), mediaDimension(item.width)},
        {QStringLiteral("height"), mediaDimension(item.height)},
    };
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputMessagePhoto")},
        {QStringLiteral("photo"), photo},
        {QStringLiteral("caption"), formattedCaption(caption)},
        {QStringLiteral("show_caption_above_media"), false},
        {QStringLiteral("self_destruct_type"), QJsonValue::Null},
        {QStringLiteral("has_spoiler"), false},
    };
}

QJsonObject inputVideoContent(const AlbumItem& item, const QString& caption)
{
    const QJsonObject video{
        {QStringLiteral("@type"), QStringLiteral("inputVideo")},
        {QStringLiteral("video"), localFile(item.path)},
        {QStringLiteral("thumbnail"), QJsonValue::Null},
        {QStringLiteral("cover"), QJsonValue::Null},
        {QStringLiteral("start_timestamp"), 0},
        {QStringLiteral("added_sticker_file_ids"), QJsonArray()},
        {QStringLiteral("duration"), 0},
        {QStringLiteral("width"), mediaDimension(item.width)},
        {QStringLiteral("height"), mediaDimension(item.height)},
        {QStringLiteral("supports_streaming"), false},
    };
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputMessageVideo")},
        {QStringLiteral("video"), video},
        {QStringLiteral("caption"), formattedCaption(caption)},
        {QStringLiteral("show_caption_above_media"), false},
        {QStringLiteral("self_destruct_type"), QJsonValue::Null},
        {QStringLiteral("has_spoiler"), false},
    };
}

bool isSendable(const QVector<AlbumItem>& items)
{
    if (items.isEmpty() || items.size() > kMaxAlbumItems)
        return false;
    for (const AlbumItem& item : items) {
        if (item.path.isEmpty() || item.path.trimmed().isEmpty())
            return false;
    }
    return true;
}

} // namespace

QJsonObject sendAlbumRequest(qint64 chatId, const QVector<AlbumItem>& items,
                             qint64 replyToMessageId)
{
    if (!isSendable(items))
        return {};

    QJsonArray contents;
    for (int index = 0; index < items.size(); ++index) {
        const AlbumItem& item = items.at(index);
        // TDLib allows a caption on every content, but an album is shown as one
        // bubble, so only the first member carries the user's caption.
        const QString caption = index == 0 ? item.caption : QString();
        contents.append(item.video ? inputVideoContent(item, caption)
                                   : inputPhotoContent(item, caption));
    }

    const QJsonValue reply = replyToMessageId > 0
        ? QJsonValue(inputReplyToMessageRequest(replyToMessageId))
        : QJsonValue(QJsonValue::Null);
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendMessageAlbum")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("reply_to"), reply},
        {QStringLiteral("options"), QJsonValue::Null},
        {QStringLiteral("input_message_contents"), contents},
    };
}

} // namespace beygram::telegram
