#include "BotCommands.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

namespace beygram::telegram {

QVector<BotCommand> botCommandsFromUserFullInfo(const QJsonObject& fullInfo)
{
    QVector<BotCommand> commands;

    // TDLib sends bot_info only for bots, so its absence is the whole
    // non-bot test: a user full info without it carries no menu.
    const QJsonValue botInfoValue = fullInfo.value(QStringLiteral("bot_info"));
    if (!botInfoValue.isObject())
        return commands;

    const QJsonArray rawCommands =
        botInfoValue.toObject().value(QStringLiteral("commands")).toArray();
    commands.reserve(rawCommands.size());

    for (const QJsonValue& value : rawCommands) {
        if (!value.isObject())
            continue;

        const QJsonObject raw = value.toObject();
        QString name = raw.value(QStringLiteral("command")).toString().trimmed();
        // Commands travel with a leading slash in some bot payloads and
        // without one in others; the menu always stores the bare name.
        if (name.startsWith(QLatin1Char('/')))
            name = name.mid(1).trimmed();
        if (name.isEmpty())
            continue;

        const QString description =
            raw.value(QStringLiteral("description")).toString().trimmed();
        commands.append(BotCommand{name, description});
    }

    return commands;
}

bool shouldShowStartButton(const QJsonObject& chat, const QJsonObject& fullInfo)
{
    if (!fullInfo.value(QStringLiteral("bot_info")).isObject())
        return false;

    const QJsonObject type = chat.value(QStringLiteral("type")).toObject();
    if (type.value(QStringLiteral("@type")).toString() != QLatin1String("chatTypePrivate"))
        return false;

    // An outgoing last message is direct proof that the user has written here.
    const QJsonObject lastMessage = chat.value(QStringLiteral("last_message")).toObject();
    if (lastMessage.value(QStringLiteral("is_outgoing")).toBool(false))
        return false;

    // An older outgoing message can sit behind a newer incoming one, so the
    // read-outbox marker is the second signal that the user has written.
    return jsonInt64(chat.value(QStringLiteral("last_read_outbox_message_id"))) <= 0;
}

QString botStartCommand(const QString& parameter)
{
    // A start payload is a single argument after /start. Line breaks and other
    // whitespace collapse to one space and the remaining control characters are
    // dropped, so the payload can never be read as a second command or a second
    // message.
    QString payload;
    payload.reserve(parameter.size());
    bool pendingSpace = false;

    for (const QChar character : parameter) {
        if (character.isSpace()) {
            pendingSpace = !payload.isEmpty();
            continue;
        }
        if (character.category() == QChar::Other_Control)
            continue;
        if (pendingSpace) {
            payload.append(QLatin1Char(' '));
            pendingSpace = false;
        }
        payload.append(character);
    }

    if (payload.isEmpty())
        return QStringLiteral("/start");

    return QStringLiteral("/start ") + payload;
}

} // namespace beygram::telegram
