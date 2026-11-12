#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Per-chat theme, mirrored from TDLib's chat theme fields.
///
/// TDLib names the theme directly on the chat (chat.theme_name) and carries the
/// resolved theme in a chatTheme object:
///   chatTheme name:string light_theme:int64 dark_theme:int64
///       accent_color:int32 outgoing_bubble_color:int32
///       message_text_color:int32 = ChatTheme;
///
/// Colors are TDLib's packed 0xRRGGBB integers. 0 means the payload leaves the
/// colour to the app, which is also what an empty or default theme reports.
struct ChatThemeState {
    /// Theme name, e.g. "day", "night", "tinted" or a custom theme's slug.
    /// Empty means the chat follows the app-wide theme.
    QString name;
    /// Id of the light variant of the theme; 0 when none is named.
    qint64 lightThemeId = 0;
    /// Id of the dark variant of the theme; 0 when none is named.
    qint64 darkThemeId = 0;
    /// Accent colour of the theme as 0xRRGGBB.
    int accentColor = 0;
    /// Background of the chat's own outgoing messages as 0xRRGGBB.
    int outgoingBubbleColor = 0;
    /// Text colour inside message bubbles as 0xRRGGBB.
    int messageTextColor = 0;
};

/// Reads the theme of one chat from a chat object.
///
/// The theme object is taken from "chatTheme" or "chat_theme" (and from
/// "theme", the member name newer update payloads use); a bare chatTheme object
/// is accepted as well. chat.theme_name is authoritative for the name and wins
/// over the theme object's own name, because TDLib keeps the per-chat choice
/// there while the object describes the resolved theme. An explicitly empty
/// theme_name therefore reports the app-wide theme even when a resolved object
/// still names one.
///
/// Missing or wrongly typed fields keep the defaults above, so an empty object
/// reports the app-wide theme. Colors accept numbers and numeric strings;
/// theme ids accept int64 strings, which the JSON interface uses for values
/// outside the double-precision range.
[[nodiscard]] ChatThemeState chatThemeFromChat(const QJsonObject& chat);

/// Reads the theme of one chat from an updateChatTheme update.
///
/// Accepts the update itself (theme_name plus either a nested chatTheme or the
/// theme fields inline), a chat object, or a bare chatTheme object. The update's
/// chat_id is not returned: the caller routes the update to a chat itself, and a
/// bare theme object has no chat id at all.
[[nodiscard]] ChatThemeState chatThemeFromUpdate(const QJsonObject& update);

/// Builds setChatTheme chat_id:int53 theme_name:string = Ok.
///
/// An empty (or whitespace-only) name resets the chat to the app-wide theme,
/// which is how TDLib spells "no per-chat theme"; the name is trimmed so a
/// stray space never turns into a theme that does not exist.
[[nodiscard]] QJsonObject setChatThemeRequest(qint64 chatId, const QString& themeName);

} // namespace beygram::telegram
