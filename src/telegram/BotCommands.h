#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One command advertised by a bot, parsed from TDLib without touching the UI.
///
/// `command` is the bare name without a leading slash ("start", not "/start"),
/// so a caller can insert it after its own slash; `description` is the trimmed
/// human text and may be empty when the bot sends none. The struct holds no
/// pointers, so it can be cached per account and copied across threads.
struct BotCommand {
    QString command;
    QString description;
};

/// Parses userFullInfo.bot_info.commands.
///
/// TDLib shape (td_api.tl):
///
///   userFullInfo ... bot_info:botInfo ...
///   botInfo short_description:string description:string photo:chatPhoto?
///           animation:animation? menu_button:botMenuButton
///           commands:vector<botCommand> ...
///   botCommand command:string description:string = BotCommand;
///
/// Commands are trimmed and de-slashed exactly as the Android client does
/// (TdBotFeatures.kt commandMenu): a single leading '/' is dropped, a name that
/// is empty after trimming is skipped, and an empty description is kept so the
/// menu can still show the command. An absent or non-object bot_info, a
/// non-array commands field, and non-object entries all yield an empty list, so
/// a full info that does not describe a bot never produces a menu. The TDLib
/// order is preserved.
[[nodiscard]] QVector<BotCommand> botCommandsFromUserFullInfo(const QJsonObject& fullInfo);

/// Whether a private bot chat should offer the explicit "Start bot" action.
///
/// Mirrors the Android BotStartBar gate (ChatScreen.kt:1994-2002): the chat must
/// be private, the full info must describe a bot (TDLib only sends bot_info for
/// bots), and the history must not contain an outgoing message. Only the chat
/// object is available here, so "no outgoing message" is read from the two
/// fields that prove one exists: last_message.is_outgoing and a non-zero
/// last_read_outbox_message_id. An empty history therefore qualifies, and so
/// does a chat where only the bot has posted so far. Groups, channels, secret
/// chats and non-bot users never qualify, because /start is a private-chat
/// action and the command menu is deliberately private-only in this client.
[[nodiscard]] bool shouldShowStartButton(const QJsonObject& chat, const QJsonObject& fullInfo);

/// Builds the text of the /start command, optionally carrying a deep-link
/// parameter: "/start" or "/start <parameter>".
///
/// The parameter is trimmed, every whitespace run (including line breaks)
/// collapses to a single space, and non-whitespace control characters are
/// dropped, so a payload can never add a second line or a second command. An
/// empty or whitespace-only parameter yields exactly "/start" with no trailing
/// space. Every other character is passed through unchanged, so a legitimate
/// non-ASCII payload survives, and a leading '/' in the parameter stays part of
/// the argument rather than being promoted to another command.
[[nodiscard]] QString botStartCommand(const QString& parameter = {});

} // namespace beygram::telegram
