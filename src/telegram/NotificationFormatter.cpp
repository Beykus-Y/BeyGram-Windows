#include "NotificationFormatter.h"

#include "TelegramTypes.h"

namespace beygram::telegram {
namespace {

// Longest body we keep before adding the ellipsis, so the result never exceeds
// 181 characters (180 plus the ellipsis).
constexpr int kMaxBodyLength = 180;
constexpr char16_t kEllipsis = 0x2026; // horizontal ellipsis

/// Only content that produces a real, user-visible notification gets a body.
/// Service messages (messageService*) and unknown types are absent on purpose,
/// so they never notify.
bool isPreviewableContent(const QString& type)
{
    return type == QLatin1String("messageText")
        || type == QLatin1String("messagePhoto")
        || type == QLatin1String("messageVideo")
        || type == QLatin1String("messageAnimation")
        || type == QLatin1String("messageDocument")
        || type == QLatin1String("messageAudio")
        || type == QLatin1String("messageVoiceNote")
        || type == QLatin1String("messageVideoNote")
        || type == QLatin1String("messageSticker")
        || type == QLatin1String("messagePoll")
        || type == QLatin1String("messageLocation")
        || type == QLatin1String("messageVenue")
        || type == QLatin1String("messageContact")
        || type == QLatin1String("messageDice")
        || type == QLatin1String("messageGame");
}

/// A short honest placeholder for content that carries no text or caption.
QString fallbackLabel(const QJsonObject& content, const QString& type)
{
    if (type == QLatin1String("messageText"))
        return QStringLiteral("Message");
    if (type == QLatin1String("messagePhoto"))
        return QStringLiteral("Photo");
    if (type == QLatin1String("messageVideo"))
        return QStringLiteral("Video");
    if (type == QLatin1String("messageAnimation"))
        return QStringLiteral("GIF");
    if (type == QLatin1String("messageDocument")) {
        const QString fileName = content.value(QStringLiteral("document"))
                                     .toObject()
                                     .value(QStringLiteral("file_name"))
                                     .toString()
                                     .trimmed();
        return fileName.isEmpty() ? QStringLiteral("Document") : fileName;
    }
    if (type == QLatin1String("messageAudio"))
        return QStringLiteral("Audio");
    if (type == QLatin1String("messageVoiceNote"))
        return QStringLiteral("Voice message");
    if (type == QLatin1String("messageVideoNote"))
        return QStringLiteral("Video message");
    if (type == QLatin1String("messageSticker"))
        return QStringLiteral("Sticker");
    if (type == QLatin1String("messagePoll"))
        return QStringLiteral("Poll");
    if (type == QLatin1String("messageLocation"))
        return QStringLiteral("Location");
    if (type == QLatin1String("messageVenue"))
        return QStringLiteral("Venue");
    if (type == QLatin1String("messageContact"))
        return QStringLiteral("Contact");
    if (type == QLatin1String("messageDice"))
        return QStringLiteral("Dice");
    if (type == QLatin1String("messageGame"))
        return QStringLiteral("Game");
    return QStringLiteral("Message");
}

QString formattedTextBody(const QJsonObject& content)
{
    const QString text = content.value(QStringLiteral("text"))
                             .toObject()
                             .value(QStringLiteral("text"))
                             .toString();
    if (!text.isEmpty())
        return text;
    return content.value(QStringLiteral("caption"))
        .toObject()
        .value(QStringLiteral("text"))
        .toString();
}

/// Trims and turns every run of whitespace (newlines included) into one space.
QString collapseWhitespace(const QString& text)
{
    QString collapsed;
    collapsed.reserve(text.size());
    bool pendingSpace = false;
    for (const QChar character : text) {
        if (character.isSpace()) {
            pendingSpace = !collapsed.isEmpty();
            continue;
        }
        if (pendingSpace) {
            collapsed.append(QLatin1Char(' '));
            pendingSpace = false;
        }
        collapsed.append(character);
    }
    return collapsed;
}

/// Cuts the body to kMaxBodyLength characters and appends an ellipsis. Never
/// splits a surrogate pair and never ends on trailing whitespace.
QString truncateBody(const QString& text)
{
    if (text.size() <= kMaxBodyLength)
        return text;

    int length = kMaxBodyLength;
    if (text.at(length - 1).isHighSurrogate())
        --length;
    QString truncated = text.left(length);
    while (!truncated.isEmpty() && truncated.at(truncated.size() - 1).isSpace())
        truncated.chop(1);
    return truncated + QChar(static_cast<char16_t>(kEllipsis));
}

} // namespace

NotificationPayload notificationForMessage(const QJsonObject& message,
                                           const QString& chatTitle, bool isPrivateChat,
                                           qint64 currentChatId, bool chatMuted)
{
    NotificationPayload payload;

    const qint64 messageId = jsonInt64(message.value(QStringLiteral("id")));
    const qint64 chatId = jsonInt64(message.value(QStringLiteral("chat_id")));
    if (messageId <= 0 || chatId <= 0)
        return payload;

    if (message.value(QStringLiteral("is_outgoing")).toBool())
        return payload;

    // Reading the chat already, or the chat is muted: no notification.
    if (chatId == currentChatId || chatMuted)
        return payload;

    const QJsonObject content = message.value(QStringLiteral("content")).toObject();
    const QString contentType = content.value(QStringLiteral("@type")).toString();
    if (!isPreviewableContent(contentType))
        return payload;

    const QString trimmedTitle = chatTitle.trimmed();
    if (!trimmedTitle.isEmpty())
        payload.title = trimmedTitle;
    else
        payload.title = isPrivateChat ? QStringLiteral("Telegram") : QStringLiteral("Chat");

    QString body = collapseWhitespace(formattedTextBody(content));
    if (body.isEmpty())
        body = collapseWhitespace(fallbackLabel(content, contentType));
    payload.body = truncateBody(body);

    payload.shouldNotify = true;
    payload.chatId = chatId;
    payload.messageId = messageId;
    return payload;
}

} // namespace beygram::telegram
