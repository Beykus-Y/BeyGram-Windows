#include "MessageSpecial.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <limits>

namespace beygram::telegram {
namespace {

QString typeOf(const QJsonObject& object)
{
    return object.value(QStringLiteral("@type")).toString();
}

/// Accepts both a bare content object and a message that carries one under
/// "content". The returned object is the content in either case.
QJsonObject contentOf(const QJsonObject& value)
{
    if (typeOf(value) == QLatin1String("message")) {
        const QJsonObject content = value.value(QStringLiteral("content")).toObject();
        if (!content.isEmpty())
            return content;
    }
    return value;
}

/// Reads a positive file id, including a string-encoded id from tdjson. Values
/// that do not fit the int file-id field are treated as unusable.
int positiveFileId(const QJsonValue& value)
{
    const qint64 id = jsonInt64(value);
    if (id <= 0 || id > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(id);
}

/// Strips the diceStickers prefix from a DiceStickers @type. A dotted class
/// name keeps only its last segment; a value that is already a suffix is
/// returned unchanged.
QString stateNameFromType(QString type)
{
    type = type.trimmed();
    if (type.isEmpty())
        return {};

    const qsizetype dot = type.lastIndexOf(QLatin1Char('.'));
    if (dot >= 0)
        type = type.mid(dot + 1);

    const QLatin1String prefix("diceStickers");
    if (type.startsWith(prefix))
        type = type.mid(prefix.size());

    return type;
}

QString stateName(const QJsonValue& value)
{
    if (value.isString())
        return stateNameFromType(value.toString());
    if (!value.isObject())
        return {};

    const QJsonObject state = value.toObject();
    QString type = state.value(QStringLiteral("@type")).toString();
    if (type.isEmpty())
        type = state.value(QStringLiteral("type")).toString();
    return stateNameFromType(type);
}

QString diceState(const QJsonObject& content)
{
    const QString state = stateName(content.value(QStringLiteral("state")));
    if (!state.isEmpty())
        return state;

    const QString finalState = stateName(content.value(QStringLiteral("final_state")));
    if (!finalState.isEmpty())
        return finalState;

    return stateName(content.value(QStringLiteral("initial_state")));
}

/// The largest photo size's file id. TDLib lists sizes from smallest to
/// largest, so the last usable entry is the fallback when a minimal wrapper
/// omits the dimensions that photoLargestFileId() needs.
int gamePhotoFileId(const QJsonObject& game)
{
    const QJsonObject photo = game.value(QStringLiteral("photo")).toObject();

    int fileId = photoLargestFileId(photo);
    if (fileId > 0)
        return fileId;

    for (const QJsonValue& value : photo.value(QStringLiteral("sizes")).toArray()) {
        const QJsonObject size = value.toObject();
        const int candidate = positiveFileId(size.value(QStringLiteral("photo"))
                                                 .toObject()
                                                 .value(QStringLiteral("id")));
        if (candidate > 0)
            fileId = candidate;
    }
    if (fileId > 0)
        return fileId;

    fileId = positiveFileId(
        photo.value(QStringLiteral("photo")).toObject().value(QStringLiteral("id")));
    if (fileId > 0)
        return fileId;

    fileId = positiveFileId(
        photo.value(QStringLiteral("file")).toObject().value(QStringLiteral("id")));
    if (fileId > 0)
        return fileId;

    return positiveFileId(photo.value(QStringLiteral("id")));
}

} // namespace

DiceContent diceFromContent(const QJsonObject& content)
{
    const QJsonObject diceContent = contentOf(content);
    if (typeOf(diceContent) != QLatin1String("messageDice"))
        return {};

    DiceContent dice;
    dice.emoji = diceContent.value(QStringLiteral("emoji")).toString().trimmed();
    if (dice.emoji.isEmpty())
        dice.emoji = QString::fromUtf8("\xF0\x9F\x8E\xB2"); // U+1F3B2

    const qint64 value = jsonInt64(diceContent.value(QStringLiteral("value")));
    dice.value = static_cast<int>(std::clamp<qint64>(value, 1, 6));
    dice.state = diceState(diceContent);
    return dice;
}

GameContent gameFromContent(const QJsonObject& content)
{
    const QJsonObject gameContent = contentOf(content);
    if (typeOf(gameContent) != QLatin1String("messageGame"))
        return {};

    const QJsonObject game = gameContent.value(QStringLiteral("game")).toObject();
    const qint64 gameId = jsonInt64(game.value(QStringLiteral("id")));
    if (gameId <= 0)
        return {};

    GameContent result;
    result.gameId = gameId;
    result.title = game.value(QStringLiteral("title")).toString();
    result.description = game.value(QStringLiteral("description")).toString();
    if (result.description.isEmpty()) {
        result.description = game.value(QStringLiteral("text"))
                                 .toObject()
                                 .value(QStringLiteral("text"))
                                 .toString();
    }
    result.photoFileId = gamePhotoFileId(game);
    return result;
}

AnimatedEmojiContent animatedEmojiFromContent(const QJsonObject& content)
{
    const QJsonObject emojiContent = contentOf(content);
    if (typeOf(emojiContent) != QLatin1String("messageAnimatedEmoji"))
        return {};

    const QJsonObject animated =
        emojiContent.value(QStringLiteral("animated_emoji")).toObject();

    QJsonObject sticker = animated.value(QStringLiteral("sticker")).toObject();
    if (sticker.isEmpty())
        sticker = emojiContent.value(QStringLiteral("sticker")).toObject();

    // TDLib puts the custom emoji id in sticker.full_type. Older and minimal
    // wrappers may carry it on the sticker, the animatedEmoji or the content.
    qint64 customEmojiId =
        jsonInt64(animated.value(QStringLiteral("custom_emoji_id")));
    if (customEmojiId <= 0)
        customEmojiId = jsonInt64(sticker.value(QStringLiteral("custom_emoji_id")));
    if (customEmojiId <= 0) {
        customEmojiId = jsonInt64(sticker.value(QStringLiteral("full_type"))
                                      .toObject()
                                      .value(QStringLiteral("custom_emoji_id")));
    }
    if (customEmojiId <= 0) {
        customEmojiId = jsonInt64(animated.value(QStringLiteral("full_type"))
                                      .toObject()
                                      .value(QStringLiteral("custom_emoji_id")));
    }
    if (customEmojiId <= 0)
        customEmojiId = jsonInt64(emojiContent.value(QStringLiteral("custom_emoji_id")));
    if (customEmojiId <= 0)
        return {};

    AnimatedEmojiContent result;
    result.customEmojiId = customEmojiId;

    int stickerFileId = positiveFileId(
        sticker.value(QStringLiteral("sticker")).toObject().value(QStringLiteral("id")));
    if (stickerFileId <= 0) {
        stickerFileId = positiveFileId(
            sticker.value(QStringLiteral("file")).toObject().value(QStringLiteral("id")));
    }
    if (stickerFileId <= 0 && typeOf(sticker) == QLatin1String("file"))
        stickerFileId = positiveFileId(sticker.value(QStringLiteral("id")));
    if (stickerFileId <= 0) {
        stickerFileId = positiveFileId(
            animated.value(QStringLiteral("sticker_file_id")));
    }
    if (stickerFileId <= 0) {
        stickerFileId = positiveFileId(
            emojiContent.value(QStringLiteral("sticker_file_id")));
    }
    if (stickerFileId <= 0)
        stickerFileId = positiveFileId(emojiContent.value(QStringLiteral("file_id")));
    result.stickerFileId = stickerFileId;

    result.alt = emojiContent.value(QStringLiteral("alt")).toString().trimmed();
    if (result.alt.isEmpty())
        result.alt = animated.value(QStringLiteral("alt")).toString().trimmed();
    if (result.alt.isEmpty())
        result.alt = emojiContent.value(QStringLiteral("emoji")).toString().trimmed();
    if (result.alt.isEmpty())
        result.alt = animated.value(QStringLiteral("emoji")).toString().trimmed();
    return result;
}

bool isSpecialContent(const QJsonObject& content)
{
    const QString type = typeOf(contentOf(content));
    return type == QLatin1String("messageDice")
        || type == QLatin1String("messageGame")
        || type == QLatin1String("messageAnimatedEmoji");
}

} // namespace beygram::telegram
