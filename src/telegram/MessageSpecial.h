#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// The parts of a messageDice content that a chat bubble needs.
///
/// emoji is the content's emoji, or U+1F3B2 when the field is missing or blank.
/// value is clamped to Telegram's 1..6 result range. state is the suffix of the
/// diceStickers @type, so diceStickersRegular becomes "Regular" and
/// diceStickersSlotMachine becomes "SlotMachine". It is empty when the content
/// carries no state. The parser reads state, then final_state, then
/// initial_state, because a server result normally arrives in final_state.
struct DiceContent {
    QString emoji;
    int value = 0;
    QString state;
};

/// Parses the content object of a messageDice message. A message envelope that
/// carries the content under "content" is accepted as well. Any other content
/// type returns the default value. Pure: no I/O, no logging.
[[nodiscard]] DiceContent diceFromContent(const QJsonObject& content);

/// The parts of a messageGame content that a chat bubble needs.
///
/// gameId is game.id and must be positive; otherwise every field keeps its
/// default value. title and description are the game's title and description
/// (falling back to game.text.text when description is absent). photoFileId is
/// the file id of the largest game.photo size, or zero when the game has no
/// usable photo.
struct GameContent {
    qint64 gameId = 0;
    QString title;
    QString description;
    int photoFileId = 0;
};

/// Parses the content object of a messageGame message. A message envelope that
/// carries the content under "content" is accepted as well. Any other content
/// type, or a game without a positive id, returns the default value. Pure: no
/// I/O, no logging.
[[nodiscard]] GameContent gameFromContent(const QJsonObject& content);

/// The parts of a messageAnimatedEmoji content that a chat bubble needs.
///
/// customEmojiId is the custom emoji identifier from the sticker's full_type.
/// It must be positive; otherwise every field keeps its default value.
/// stickerFileId is the id of the sticker's file, not the sticker id, and is
/// zero when the content carries no usable file. alt is the trimmed emoji text.
struct AnimatedEmojiContent {
    qint64 customEmojiId = 0;
    int stickerFileId = 0;
    QString alt;
};

/// Parses the content object of a messageAnimatedEmoji message. A message
/// envelope that carries the content under "content" is accepted as well. Any
/// other content type, or an animated emoji without a positive custom emoji id,
/// returns the default value. Pure: no I/O, no logging.
[[nodiscard]] AnimatedEmojiContent animatedEmojiFromContent(const QJsonObject& content);

/// True when the object is tagged messageDice, messageGame or
/// messageAnimatedEmoji, including when it is a message envelope carrying one
/// of them under "content". The check is by content type only: malformed fields
/// do not make content unspecial.
[[nodiscard]] bool isSpecialContent(const QJsonObject& content);

} // namespace beygram::telegram
