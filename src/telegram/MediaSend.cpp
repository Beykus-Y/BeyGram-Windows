#include "MediaSend.h"

#include "ReplyInfo.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>

namespace beygram::telegram {
namespace {

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

QJsonObject inputPhotoContent(const QString& localPath, const QString& caption)
{
    const QJsonObject photo{
        {QStringLiteral("@type"), QStringLiteral("inputPhoto")},
        {QStringLiteral("photo"), localFile(localPath)},
        {QStringLiteral("thumbnail"), QJsonValue::Null},
        {QStringLiteral("video"), QJsonValue::Null},
        {QStringLiteral("added_sticker_file_ids"), QJsonArray()},
        {QStringLiteral("width"), 0},
        {QStringLiteral("height"), 0},
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

QJsonObject inputVideoContent(const QString& localPath, const QString& caption)
{
    const QJsonObject video{
        {QStringLiteral("@type"), QStringLiteral("inputVideo")},
        {QStringLiteral("video"), localFile(localPath)},
        {QStringLiteral("thumbnail"), QJsonValue::Null},
        {QStringLiteral("cover"), QJsonValue::Null},
        {QStringLiteral("start_timestamp"), 0},
        {QStringLiteral("added_sticker_file_ids"), QJsonArray()},
        {QStringLiteral("duration"), 0},
        {QStringLiteral("width"), 0},
        {QStringLiteral("height"), 0},
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

QJsonObject inputDocumentContent(const QString& localPath, const QString& caption)
{
    // This TDLib schema spells the "send as file" flag
    // disable_content_type_detection (older schemas used force_file).
    const QJsonObject document{
        {QStringLiteral("@type"), QStringLiteral("inputDocument")},
        {QStringLiteral("document"), localFile(localPath)},
        {QStringLiteral("thumbnail"), QJsonValue::Null},
        {QStringLiteral("disable_content_type_detection"), false},
    };
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputMessageDocument")},
        {QStringLiteral("document"), document},
        {QStringLiteral("caption"), formattedCaption(caption)},
    };
}

QJsonObject inputMessageContent(OutgoingMediaKind kind, const QString& localPath,
                                const QString& caption)
{
    switch (kind) {
    case OutgoingMediaKind::Photo:
        return inputPhotoContent(localPath, caption);
    case OutgoingMediaKind::Video:
        return inputVideoContent(localPath, caption);
    case OutgoingMediaKind::Document:
        return inputDocumentContent(localPath, caption);
    }
    return inputDocumentContent(localPath, caption);
}

QStringList allowedExtensions(OutgoingMediaKind kind)
{
    switch (kind) {
    case OutgoingMediaKind::Photo:
        return {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
                QStringLiteral("webp")};
    case OutgoingMediaKind::Video:
        return {QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("mov"),
                QStringLiteral("webm")};
    case OutgoingMediaKind::Document:
        break;
    }
    return {};
}

} // namespace

QJsonObject sendMediaMessageRequest(qint64 chatId, OutgoingMediaKind kind,
                                   const QString& localPath, const QString& caption,
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
        {QStringLiteral("input_message_content"),
         inputMessageContent(kind, localPath, caption)},
    };
}

bool isSupportedMediaPath(const QString& localPath, OutgoingMediaKind kind)
{
    if (localPath.isEmpty() || localPath.trimmed().isEmpty())
        return false;
    if (localPath.contains(QStringLiteral("://")))
        return false;

    const QFileInfo info(localPath);
    if (!info.isFile())
        return false;

    const QString suffix = info.suffix();
    if (suffix.isEmpty())
        return false;
    if (kind == OutgoingMediaKind::Document)
        return true;

    return allowedExtensions(kind).contains(suffix.toLower());
}

} // namespace beygram::telegram
