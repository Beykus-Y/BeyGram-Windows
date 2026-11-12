#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// One Web App (Mini App) attached to a TDLib message, parsed without touching
/// the network or the UI.
///
/// The description is read from a `web_app` object, or from its camelCase
/// spelling `webApp`, on the message itself and then on message["content"].
/// The object is expected to carry `url`, `title`, `description` and
/// `bot_user_id`; nothing is filled with placeholder text, so a missing field
/// stays empty and a caller can decide what to show.
struct WebAppInfo {
    /// Bot that owns the Web App. Read from the description, falling back to
    /// message["via_bot_id"], which is the only bot id a message carries when
    /// the button itself omits one. 0 means "unknown"; openWebAppRequest then
    /// refuses to build a launch.
    qint64 botUserId = 0;

    /// The URL as TDLib sent it. It is never upgraded or rewritten here; use
    /// isSafeWebAppUrl() (or isSecure below) before opening it.
    QString url;

    QString title;

    /// Plain text. A formattedText object is accepted as well and contributes
    /// its "text", so both current and older TDLib shapes parse.
    QString description;

    /// True only when url passes isSafeWebAppUrl(): https, a host, no
    /// credentials. Every value produced by webAppFromMessage() is computed
    /// from the parsed URL, so an empty or unsafe URL reports false.
    bool isSecure = true;
};

/// Parses the Web App description of a message.
///
/// Lookup order for the description object: message["web_app"], then
/// message["webApp"], then content["web_app"], then content["webApp"]. A
/// message without such an object yields a default-constructed WebAppInfo
/// (every field empty, botUserId 0, isSecure true as declared).
///
/// Never logs. A description that exists but has no URL is still reported, so
/// the title and description can be displayed; isSecure is then false and the
/// caller must not open it.
[[nodiscard]] WebAppInfo webAppFromMessage(const QJsonObject& message);

/// Builds TDLib openWebApp for an inline-keyboard Web App button.
///
/// chatId is the chat that owns the message, botUserId the bot that owns the
/// Web App. Returns an empty QJsonObject when either id is not positive or when
/// url fails isSafeWebAppUrl(), so a malformed button can never be launched.
///
/// The URL is trimmed before it is sent, so surrounding whitespace never
/// reaches TDLib; it is otherwise passed unchanged. The forum topic and the
/// replied-to message are not part of this signature and are sent as null (the
/// chat's default topic, no reply). The open parameters carry the application
/// name "BeyGram" and the full-size mode; the theme is null because the palette
/// belongs to the UI layer, which replaces it before the request is sent.
[[nodiscard]] QJsonObject openWebAppRequest(qint64 chatId, qint64 botUserId,
                                            const QString& url);

/// True when url may be handed to a Web App view.
///
/// Only https is accepted; a scheme-less, http, tg or javascript URL is never
/// upgraded or guessed. The authority must have a non-empty host and no
/// userinfo, so "https://user:pass@host/" and "https://host@evil.example/" are
/// rejected; a port, when present, must be numeric and inside 1..65535. The
/// value is trimmed, may not contain inner whitespace or control characters,
/// and is capped at 2048 characters. Host characters are limited to ASCII
/// letters, digits, '-' and '.', with labels of at most 63 characters, plus a
/// bracketed IPv6 literal; IDN hosts must already be punycode.
///
/// Never logs and never performs a lookup.
[[nodiscard]] bool isSafeWebAppUrl(const QString& url);

} // namespace beygram::telegram
