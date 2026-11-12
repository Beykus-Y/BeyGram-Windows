#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One button of a bot keyboard. Parsed from TDLib without touching the UI, so
/// the model layer can hold it directly.
///
/// `type` is the TDLib variant name without its class prefix, so a caller can
/// switch on it without carrying the TDLib class names around: "text",
/// "requestPhoneNumber", "requestLocation", "requestPoll", "requestUsers",
/// "requestChat", "webApp", "callback", "callbackWithPassword", "callbackGame",
/// "switchInline", "url", "loginUrl", "copyText", "user", "buy", or
/// "unsupported" for a variant this build does not know.
struct BotButton {
    QString text;
    QString type;
    /// Callback payload as TDLib sends it (base64 for a bytes field), the text
    /// of a copyText button, or the query of a switchInline button.
    QString data;
    /// Target of url, loginUrl and webApp buttons.
    QString url;
    /// Target of a user button.
    qint64 userId = 0;
};

/// The bot keyboard carried by one message. Exactly one of `replyKeyboard` and
/// `inlineKeyboard` is set; both stay false when the message carries no
/// keyboard this layer renders (missing markup, force reply, remove keyboard).
struct BotKeyboard {
    bool replyKeyboard = false;
    bool inlineKeyboard = false;
    bool oneTime = false;
    /// Mirrors TDLib's resize_keyboard, which defaults to true.
    bool resize = true;
    /// input_field_placeholder, empty when the bot did not ask for one.
    QString placeholder;
    /// Rows of buttons in TDLib order. Rows without a usable button are
    /// dropped, so every row here is renderable.
    QVector<QVector<BotButton>> rows;

    [[nodiscard]] bool isEmpty() const { return rows.isEmpty(); }
};

/// Parses message["reply_markup"]. Handles replyMarkupShowKeyboard and
/// replyMarkupInlineKeyboard; any other markup yields an empty keyboard.
[[nodiscard]] BotKeyboard botMarkupFromMessage(const QJsonObject& message);

/// Builds TDLib getCallbackQueryAnswer for a callback button press. `data` is
/// the payload as it was read from the button, TDLib's base64 bytes string.
[[nodiscard]] QJsonObject callbackQueryRequest(qint64 chatId, qint64 messageId,
                                               const QString& data);

} // namespace beygram::telegram
