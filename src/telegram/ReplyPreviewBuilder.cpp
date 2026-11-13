#include "ReplyPreviewBuilder.h"

#include "ReplyInfo.h"

#include <QJsonValue>

namespace beygram::telegram {
namespace {

QString contentText(const QJsonObject& content)
{
    const QString type = content.value(QStringLiteral("@type")).toString();
    if (type == QLatin1String("messageText")) {
        return content.value(QStringLiteral("text"))
            .toObject()
            .value(QStringLiteral("text"))
            .toString();
    }

    return content.value(QStringLiteral("caption"))
        .toObject()
        .value(QStringLiteral("text"))
        .toString();
}

QString capPreview(QString text, int maxLength)
{
    if (maxLength <= 0)
        return {};

    if (text.size() > maxLength) {
        qsizetype cut = maxLength;
        // A high surrogate at the cut needs its low surrogate, which would
        // exceed the cap; drop the pair instead of returning half a character.
        if (text.at(cut - 1).isHighSurrogate())
            --cut;
        text.truncate(cut);
    }

    // Truncation can expose a trailing space that the initial trim removed.
    return text.trimmed();
}

} // namespace

QString replyPreviewForMessage(const QJsonObject& message, int maxLength)
{
    const QJsonObject content = message.value(QStringLiteral("content")).toObject();

    QString preview = contentText(content).simplified();
    if (preview.isEmpty() && content.isEmpty())
        preview = replyTargetFromMessage(message).quoteText.simplified();
    if (preview.isEmpty())
        preview = mediaLabelForContent(content);

    return capPreview(preview, maxLength);
}

QString replySenderLabel(const QJsonObject& message, bool outgoing)
{
    if (outgoing)
        return QStringLiteral("You");

    QString name = message.value(QStringLiteral("sender_name")).toString().simplified();
    if (name.isEmpty())
        name = message.value(QStringLiteral("author_signature")).toString().simplified();
    return name;
}

QString mediaLabelForContent(const QJsonObject& content)
{
    const QString type = content.value(QStringLiteral("@type")).toString();

    if (type == QLatin1String("messagePhoto"))
        return QStringLiteral("Photo");
    if (type == QLatin1String("messageVideo"))
        return QStringLiteral("Video");
    if (type == QLatin1String("messageAnimation"))
        return QStringLiteral("GIF");
    if (type == QLatin1String("messageAudio"))
        return QStringLiteral("Audio");
    if (type == QLatin1String("messageVoiceNote"))
        return QStringLiteral("Voice message");
    if (type == QLatin1String("messageDocument"))
        return QStringLiteral("Document");
    if (type == QLatin1String("messageSticker"))
        return QStringLiteral("Sticker");
    if (type == QLatin1String("messageLocation"))
        return QStringLiteral("Location");
    if (type == QLatin1String("messageContact"))
        return QStringLiteral("Contact");
    if (type == QLatin1String("messagePoll"))
        return QStringLiteral("Poll");
    if (type == QLatin1String("messageGame"))
        return QStringLiteral("Game");
    if (type == QLatin1String("messageDice"))
        return QStringLiteral("Dice");

    return QStringLiteral("Message");
}

} // namespace beygram::telegram
