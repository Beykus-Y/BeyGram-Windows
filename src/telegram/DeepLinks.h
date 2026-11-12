#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// One Telegram entity link, parsed without touching the network.
///
/// This mirrors the fail-closed parser of the Android client
/// (app/src/main/java/com/beykus/beygram/deeplink/TelegramDeepLink.kt), reduced
/// to the routes the Windows client can act on today:
///
///   https://t.me/<username>                    -> PublicChat
///   https://t.me/<username>/<post>             -> Message
///   https://t.me/<username>/<topic>/<post>     -> Message
///   https://t.me/+<invite>                     -> Invite
///   https://t.me/+<phone>                      -> Phone
///   https://t.me/joinchat/<invite>             -> Invite
///   tg://resolve?domain=<username>             -> PublicChat
///   tg://resolve?domain=<username>&post=<id>   -> Message
///   tg://resolve?domain=<bot>&start=<payload>  -> BotStart
///   tg://resolve?phone=<number>                -> Phone
///   tg://join?invite=<invite>                  -> Invite
///   tg://user?id=<id>                          -> PrivateChat
///   tg://settings                              -> Settings
///
/// Everything else is Kind::Unknown with every field left empty. That includes
/// javascript:, http:// (the parser never guesses or upgrades a scheme), any
/// non-Telegram host, reserved routes (/share, /addstickers, /proxy, ...) and
/// every tg:// action this client cannot express (/proxy, /socks, /login,
/// /addstickers, ...). The parser never logs and never fills a field with
/// unvalidated input, so an unknown link cannot be turned into a lookup.
///
/// Input rules, copied from the Android parser: the link is trimmed, must be at
/// most 2048 characters, and may not contain whitespace or control characters
/// once trimmed. Query parsing is strict: duplicate keys, a key without '=', a
/// malformed percent escape and a value that is not valid UTF-8 all reject the
/// whole link.
struct DeepLink {
    enum class Kind {
        Unknown,
        PublicChat,
        PrivateChat,
        Message,
        BotStart,
        Invite,
        Phone,
        Settings,
    };

    Kind kind = Kind::Unknown;

    /// The route payload, without the syntax that introduced it:
    ///   PublicChat, Message, BotStart -> public username, no leading '@'
    ///   Invite                        -> invite hash, no leading '+'
    ///   Phone                         -> number in E.164 with a leading '+'
    ///   PrivateChat                   -> decimal user id
    ///   Settings, Unknown             -> empty
    QString value;

    /// The /start payload of a BotStart link. It is carried, never sent
    /// automatically: the caller opens the bot and lets the user press Start.
    QString botStartParameter;

    /// The post id of Kind::Message; 0 for every other kind.
    ///
    /// The forum topic id of a three-segment link is validated but not carried:
    /// message ids are unique inside the whole chat, so value + messageId
    /// already identifies the target message.
    qint64 messageId = 0;
};

/// Parses one t.me / telegram.me / tg:// link. Only https on the Telegram link
/// hosts (t.me and telegram.me, with or without the "www." prefix) and the tg
/// scheme are accepted; a scheme-less or http link is never upgraded.
///
/// Never performs a lookup and never logs; a malformed, foreign or unsupported
/// link yields a default DeepLink, i.e. Kind::Unknown with empty fields.
[[nodiscard]] DeepLink parseDeepLink(const QString& url);

/// Builds searchPublicChat username:string = Chat for a public username.
///
/// A leading '@' is stripped, so both "durov" and "@durov" work. Anything that
/// is not a plausible public username yields an empty QJsonObject, so a typo or
/// a pasted URL is never sent as a lookup.
[[nodiscard]] QJsonObject resolvePublicChatRequest(const QString& username);

/// Builds joinChatByInviteLink invite_link:string = Chat from an invite link
/// (https://t.me/+<hash>, https://t.me/joinchat/<hash>, tg://join?invite=<hash>)
/// or from a bare hash with or without a leading '+'.
///
/// The link is canonicalised to https://t.me/+<hash>, so untrusted input is
/// never handed to TDLib verbatim. Anything that is not an invite - including a
/// phone link - yields an empty QJsonObject; joining still needs the user's
/// confirmation, which is the caller's job.
[[nodiscard]] QJsonObject joinChatRequest(const QString& inviteLink);

} // namespace beygram::telegram
