#include "TelegramTypes.h"

#include "ReplyInfo.h"
#include "TextFormatting.h"

#include <QJsonArray>
#include <QStringList>

#include <limits>

namespace beygram::telegram {

qint64 jsonInt64(const QJsonValue& value)
{
    if (value.isString())
        return value.toString().toLongLong();
    if (value.isDouble())
        return value.toVariant().toLongLong();
    return 0;
}

int photoThumbnailFileId(const QJsonObject& photo, int targetSize, int* width, int* height)
{
    const QJsonArray sizes = photo.value(QStringLiteral("sizes")).toArray();
    int bestId = 0;
    int bestWidth = 0;
    int bestHeight = 0;
    int bestScore = std::numeric_limits<int>::max();
    int largestId = 0;
    int largestArea = 0;

    for (const QJsonValue& value : sizes) {
        const QJsonObject size = value.toObject();
        const QJsonObject file = size.value(QStringLiteral("photo")).toObject();
        const int fileId = file.value(QStringLiteral("id")).toInt();
        if (fileId == 0)
            continue;
        const int widthValue = size.value(QStringLiteral("width")).toInt();
        const int heightValue = size.value(QStringLiteral("height")).toInt();
        const int area = widthValue * heightValue;
        if (area > largestArea) {
            largestArea = area;
            largestId = fileId;
        }

        const int score = widthValue >= targetSize ? widthValue - targetSize
                                                   : (targetSize - widthValue) * 4;
        if (score < bestScore) {
            bestScore = score;
            bestId = fileId;
            bestWidth = widthValue;
            bestHeight = heightValue;
        }
    }

    if (bestId == 0)
        bestId = largestId;
    if (width)
        *width = bestWidth;
    if (height)
        *height = bestHeight;
    return bestId;
}

int photoLargestFileId(const QJsonObject& photo)
{
    const QJsonArray sizes = photo.value(QStringLiteral("sizes")).toArray();
    int largestId = 0;
    int largestArea = 0;
    for (const QJsonValue& value : sizes) {
        const QJsonObject size = value.toObject();
        const int area = size.value(QStringLiteral("width")).toInt()
            * size.value(QStringLiteral("height")).toInt();
        if (area >= largestArea) {
            largestArea = area;
            largestId = size.value(QStringLiteral("photo"))
                            .toObject()
                            .value(QStringLiteral("id"))
                            .toInt();
        }
    }
    return largestId;
}

int messageThumbnailFileId(const QJsonObject& content, int* width, int* height)
{
    const QString type = content.value(QStringLiteral("@type")).toString();

    if (type == QLatin1String("messagePhoto"))
        return photoThumbnailFileId(content.value(QStringLiteral("photo")).toObject(), 320,
                                    width, height);

    if (type == QLatin1String("messageVideo")) {
        const QJsonObject video = content.value(QStringLiteral("video")).toObject();
        if (width)
            *width = video.value(QStringLiteral("width")).toInt();
        if (height)
            *height = video.value(QStringLiteral("height")).toInt();
        return video.value(QStringLiteral("thumbnail"))
            .toObject()
            .value(QStringLiteral("file"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    }

    if (type == QLatin1String("messageAnimation")) {
        const QJsonObject animation = content.value(QStringLiteral("animation")).toObject();
        if (width)
            *width = animation.value(QStringLiteral("width")).toInt();
        if (height)
            *height = animation.value(QStringLiteral("height")).toInt();
        return animation.value(QStringLiteral("thumbnail"))
            .toObject()
            .value(QStringLiteral("file"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    }

    if (type == QLatin1String("messageSticker")) {
        const QJsonObject sticker = content.value(QStringLiteral("sticker")).toObject();
        if (width)
            *width = sticker.value(QStringLiteral("width")).toInt();
        if (height)
            *height = sticker.value(QStringLiteral("height")).toInt();
        return sticker.value(QStringLiteral("thumbnail"))
            .toObject()
            .value(QStringLiteral("file"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    }

    if (type == QLatin1String("messageDocument")) {
        const QJsonObject document = content.value(QStringLiteral("document")).toObject();
        const QJsonObject thumbnail = document.value(QStringLiteral("thumbnail")).toObject();
        if (width)
            *width = thumbnail.value(QStringLiteral("width")).toInt();
        if (height)
            *height = thumbnail.value(QStringLiteral("height")).toInt();
        return thumbnail.value(QStringLiteral("file"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    }

    if (type == QLatin1String("messageAudio")) {
        const QJsonObject audio = content.value(QStringLiteral("audio")).toObject();
        const QJsonObject thumbnail = audio.value(QStringLiteral("album_cover_thumbnail")).toObject();
        if (width)
            *width = thumbnail.value(QStringLiteral("width")).toInt();
        if (height)
            *height = thumbnail.value(QStringLiteral("height")).toInt();
        return thumbnail.value(QStringLiteral("file"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    }

    if (type == QLatin1String("messageVideoNote")) {
        const QJsonObject note = content.value(QStringLiteral("video_note")).toObject();
        const int size = note.value(QStringLiteral("length")).toInt();
        if (width)
            *width = size;
        if (height)
            *height = size;
        return note.value(QStringLiteral("thumbnail"))
            .toObject()
            .value(QStringLiteral("file"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    }

    return 0;
}

int messageFullFileId(const QJsonObject& content)
{
    const QString type = content.value(QStringLiteral("@type")).toString();
    if (type == QLatin1String("messagePhoto"))
        return photoLargestFileId(content.value(QStringLiteral("photo")).toObject());
    if (type == QLatin1String("messageVideo"))
        return content.value(QStringLiteral("video"))
            .toObject()
            .value(QStringLiteral("video"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    if (type == QLatin1String("messageAnimation"))
        return content.value(QStringLiteral("animation"))
            .toObject()
            .value(QStringLiteral("animation"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    if (type == QLatin1String("messageDocument"))
        return content.value(QStringLiteral("document"))
            .toObject()
            .value(QStringLiteral("document"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    if (type == QLatin1String("messageAudio"))
        return content.value(QStringLiteral("audio"))
            .toObject()
            .value(QStringLiteral("audio"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    if (type == QLatin1String("messageVoiceNote"))
        return content.value(QStringLiteral("voice_note"))
            .toObject()
            .value(QStringLiteral("voice"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    if (type == QLatin1String("messageVideoNote"))
        return content.value(QStringLiteral("video_note"))
            .toObject()
            .value(QStringLiteral("video"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    if (type == QLatin1String("messageSticker"))
        return content.value(QStringLiteral("sticker"))
            .toObject()
            .value(QStringLiteral("sticker"))
            .toObject()
            .value(QStringLiteral("id"))
            .toInt();
    return 0;
}

QString messageContentText(const QJsonObject& content)
{
    const QString type = content.value(QStringLiteral("@type")).toString();

    if (type == QLatin1String("messageText")) {
        return content.value(QStringLiteral("text"))
            .toObject()
            .value(QStringLiteral("text"))
            .toString();
    }
    if (type == QLatin1String("messagePhoto") || type == QLatin1String("messageVideo")
        || type == QLatin1String("messageAnimation")
        || type == QLatin1String("messageAudio")
        || type == QLatin1String("messageVoiceNote")
        || type == QLatin1String("messageDocument")) {
        const QString caption = content.value(QStringLiteral("caption"))
                                    .toObject()
                                    .value(QStringLiteral("text"))
                                    .toString();
        if (!caption.isEmpty())
            return caption;
    }
    if (type == QLatin1String("messagePoll")) {
        return content.value(QStringLiteral("poll"))
            .toObject()
            .value(QStringLiteral("question"))
            .toObject()
            .value(QStringLiteral("text"))
            .toString();
    }
    if (type == QLatin1String("messageSticker")) {
        const QJsonObject sticker = content.value(QStringLiteral("sticker")).toObject();
        const QString emoji = sticker.value(QStringLiteral("emoji")).toString();
        return emoji.isEmpty() ? QStringLiteral("Sticker") : emoji;
    }
    if (type == QLatin1String("messageDice")) {
        return content.value(QStringLiteral("emoji")).toString(QStringLiteral("Dice"));
    }
    if (type == QLatin1String("messageLocation"))
        return QStringLiteral("Location");
    if (type == QLatin1String("messageVenue")) {
        return content.value(QStringLiteral("venue"))
            .toObject()
            .value(QStringLiteral("title"))
            .toString(QStringLiteral("Venue"));
    }
    if (type == QLatin1String("messageContact"))
        return QStringLiteral("Contact");
    if (type == QLatin1String("messageCall"))
        return QStringLiteral("Call");
    if (type == QLatin1String("messageAnimatedEmoji"))
        return content.value(QStringLiteral("emoji")).toString();

    return {};
}

QString messageContentLabel(const QJsonObject& content)
{
    const QString type = content.value(QStringLiteral("@type")).toString();
    if (type == QLatin1String("messageText"))
        return QStringLiteral("text");
    if (type == QLatin1String("messagePhoto"))
        return QStringLiteral("photo");
    if (type == QLatin1String("messageVideo"))
        return QStringLiteral("video");
    if (type == QLatin1String("messageAnimation"))
        return QStringLiteral("animation");
    if (type == QLatin1String("messageAudio"))
        return QStringLiteral("audio");
    if (type == QLatin1String("messageVoiceNote"))
        return QStringLiteral("voice");
    if (type == QLatin1String("messageVideoNote"))
        return QStringLiteral("video_note");
    if (type == QLatin1String("messageDocument"))
        return QStringLiteral("document");
    if (type == QLatin1String("messageSticker"))
        return QStringLiteral("sticker");
    if (type == QLatin1String("messagePoll"))
        return QStringLiteral("poll");
    if (type.startsWith(QLatin1String("messageBasicGroup"))
        || type.startsWith(QLatin1String("messageSupergroup"))
        || type.startsWith(QLatin1String("messageChat"))
        || type == QLatin1String("messagePinMessage")
        || type == QLatin1String("messageCustomServiceAction")) {
        return QStringLiteral("service");
    }
    return type.mid(QStringLiteral("message").size()).toLower();
}

QString chatPreview(const QJsonObject& lastMessage)
{
    if (lastMessage.isEmpty())
        return {};

    QString text = messageContentText(lastMessage.value(QStringLiteral("content")).toObject());
    if (text.isEmpty()) {
        const QString type =
            lastMessage.value(QStringLiteral("content")).toObject().value(QStringLiteral("@type")).toString();
        if (type == QLatin1String("messagePhoto"))
            text = QStringLiteral("Photo");
        else if (type == QLatin1String("messageVideo"))
            text = QStringLiteral("Video");
        else if (type == QLatin1String("messageAnimation"))
            text = QStringLiteral("GIF");
        else if (type == QLatin1String("messageVoiceNote"))
            text = QStringLiteral("Voice message");
        else if (type == QLatin1String("messageVideoNote"))
            text = QStringLiteral("Video message");
        else if (type == QLatin1String("messageDocument"))
            text = lastMessage.value(QStringLiteral("content"))
                       .toObject()
                       .value(QStringLiteral("document"))
                       .toObject()
                       .value(QStringLiteral("file_name"))
                       .toString(QStringLiteral("Document"));
        else if (type == QLatin1String("messageSticker"))
            text = QStringLiteral("Sticker");
        else if (type == QLatin1String("messagePoll"))
            text = QStringLiteral("Poll");
        else if (type == QLatin1String("messageCall"))
            text = QStringLiteral("Call");
        else if (type.startsWith(QLatin1String("messageBasicGroup"))
                 || type.startsWith(QLatin1String("messageSupergroup")))
            text = QStringLiteral("Service message");
    }

    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    if (lastMessage.value(QStringLiteral("is_outgoing")).toBool() && !text.isEmpty())
        text.prepend(QStringLiteral("You: "));
    return text;
}

ChatData chatFromJson(const QJsonObject& chat)
{
    ChatData data;
    data.id = jsonInt64(chat.value(QStringLiteral("id")));
    data.title = chat.value(QStringLiteral("title")).toString();
    data.type = chat.value(QStringLiteral("type"))
                    .toObject()
                    .value(QStringLiteral("@type"))
                    .toString();
    data.isChannel = data.type == QLatin1String("chatTypeSupergroup")
        && chat.value(QStringLiteral("type"))
               .toObject()
               .value(QStringLiteral("is_channel"))
               .toBool();
    data.unreadCount = chat.value(QStringLiteral("unread_count")).toInt();
    data.muted = chat.value(QStringLiteral("notification_settings"))
                     .toObject()
                     .value(QStringLiteral("mute_for"))
                     .toInt()
        > 0;

    const QJsonObject lastMessage = chat.value(QStringLiteral("last_message")).toObject();
    if (!lastMessage.isEmpty()) {
        data.lastMessageId = jsonInt64(lastMessage.value(QStringLiteral("id")));
        data.lastMessageDate = jsonInt64(lastMessage.value(QStringLiteral("date")));
        data.preview = chatPreview(lastMessage);
    }

    const QJsonObject photo = chat.value(QStringLiteral("photo")).toObject();
    if (!photo.isEmpty()) {
        data.photoFileId = photo.value(QStringLiteral("small"))
                               .toObject()
                               .value(QStringLiteral("id"))
                               .toInt();
    }

    const QJsonArray positions = chat.value(QStringLiteral("positions")).toArray();
    for (const QJsonValue& value : positions) {
        const QJsonObject position = value.toObject();
        if (position.value(QStringLiteral("list"))
                .toObject()
                .value(QStringLiteral("@type"))
                .toString()
            == QLatin1String("chatListMain")) {
            applyChatPosition(data, position);
        }
    }

    data.initials = initialsForTitle(data.title);
    data.accentIndex = accentIndexForId(data.id);
    return data;
}

void applyChatPosition(ChatData& chat, const QJsonObject& position)
{
    const QString listType = position.value(QStringLiteral("list"))
                                 .toObject()
                                 .value(QStringLiteral("@type"))
                                 .toString();
    if (listType != QLatin1String("chatListMain"))
        return;

    const qint64 order = jsonInt64(position.value(QStringLiteral("order")));
    chat.order = order;
    chat.pinned = position.value(QStringLiteral("is_pinned")).toBool();
    chat.inMainList = order != 0;
}

MessageData messageFromJson(const QJsonObject& message)
{
    MessageData data;
    data.id = jsonInt64(message.value(QStringLiteral("id")));
    data.chatId = jsonInt64(message.value(QStringLiteral("chat_id")));
    data.date = jsonInt64(message.value(QStringLiteral("date")));
    data.outgoing = message.value(QStringLiteral("is_outgoing")).toBool();
    data.edited = jsonInt64(message.value(QStringLiteral("edit_date"))) > 0;

    const QJsonObject sender = message.value(QStringLiteral("sender_id")).toObject();
    if (sender.value(QStringLiteral("@type")).toString() == QLatin1String("messageSenderUser"))
        data.senderUserId = jsonInt64(sender.value(QStringLiteral("user_id")));

    const QJsonObject content = message.value(QStringLiteral("content")).toObject();
    data.contentType = messageContentLabel(content);
    data.text = messageContentText(content);
    const QJsonObject formatted =
        content.contains(QStringLiteral("text"))
        ? content.value(QStringLiteral("text")).toObject()
        : content.value(QStringLiteral("caption")).toObject();
    data.textHtml = formatTdlibText(formatted).html;
    data.mediaFileId = messageThumbnailFileId(content, &data.mediaWidth, &data.mediaHeight);
    const ReplyTarget reply = replyTargetFromMessage(message);
    if (reply.isValid()) {
        data.replyToMessageId = reply.messageId;
        data.replyChatId = reply.chatId != 0 ? reply.chatId : data.chatId;
        data.replyPreview = reply.quoteText;
    }
    data.mediaFullFileId = messageFullFileId(content);
    if (data.text.isEmpty()) {
        if (data.contentType == QLatin1String("photo"))
            data.text = QStringLiteral("[Photo]");
        else if (data.contentType == QLatin1String("video"))
            data.text = QStringLiteral("[Video]");
        else if (data.contentType == QLatin1String("voice"))
            data.text = QStringLiteral("[Voice message]");
        else if (data.contentType == QLatin1String("video_note"))
            data.text = QStringLiteral("[Video message]");
        else if (data.contentType == QLatin1String("document"))
            data.text = QStringLiteral("[Document]");
        else if (data.contentType == QLatin1String("sticker"))
            data.text = QStringLiteral("[Sticker]");
        else if (data.contentType == QLatin1String("poll"))
            data.text = QStringLiteral("[Poll]");
        else if (data.contentType == QLatin1String("service"))
            data.text = QStringLiteral("[Service message]");
        else
            data.text = QStringLiteral("[%1]").arg(data.contentType);
    }

    const QJsonObject sendingState = message.value(QStringLiteral("sending_state")).toObject();
    const QString stateType = sendingState.value(QStringLiteral("@type")).toString();
    data.pending = stateType == QLatin1String("messageSendingStatePending");
    data.failed = stateType == QLatin1String("messageSendingStateFailed");

    if (data.outgoing)
        data.senderName = QStringLiteral("You");
    return data;
}

QString initialsForTitle(const QString& title)
{
    const QString trimmed = title.trimmed();
    if (trimmed.isEmpty())
        return QStringLiteral("?");
    return trimmed.left(1).toUpper();
}

int accentIndexForId(qint64 id)
{
    return static_cast<int>(qAbs(id) % 6);
}

} // namespace beygram::telegram
