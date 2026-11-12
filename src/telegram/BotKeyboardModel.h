#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One button of a bot keyboard, flattened for rendering.
///
/// `type` is one of "callback", "url", "webapp", "switchInline" or "text".
/// `value` carries the callback payload, the target URL of a url/webapp button,
/// or the query of a switchInline button; it stays empty for text buttons.
struct BotKeyboardButton {
    QString text;
    QString type;
    QString value;

    friend bool operator==(const BotKeyboardButton&, const BotKeyboardButton&) = default;
};

/// Holds the keyboard carried by one bot message.
///
/// The model is a plain value holder: it parses a TDLib reply_markup object into
/// renderable rows and answers visibility questions without touching TDLib or
/// the UI. Buttons without text and variants this model cannot act on are
/// dropped, so every stored button is pressable. Row and column order follow
/// the markup.
class BotKeyboardModel final : public QObject {
    Q_OBJECT

public:
    explicit BotKeyboardModel(QObject* parent = nullptr);

    /// Replaces the stored keyboard from one message's reply_markup object.
    /// replyMarkupShowKeyboard and replyMarkupInlineKeyboard provide the rows;
    /// replyMarkupForceReply sets forceReply() and stores no rows. Any other
    /// markup (remove keyboard, missing, unknown) leaves both empty.
    ///
    /// Emits changed() only when the parsed rows or the force-reply flag differ
    /// from the stored state.
    void setReplyMarkup(const QJsonObject& markup);

    /// Returns a copy of the stored rows, in markup order.
    [[nodiscard]] QVector<QVector<BotKeyboardButton>> rows() const;

    [[nodiscard]] int rowCount() const;

    /// True when at least one row is stored.
    [[nodiscard]] bool visible() const;

    /// True when the markup asked the client to show a reply input.
    [[nodiscard]] bool forceReply() const;

    /// Drops the rows and the force-reply flag. Emits changed() once when there
    /// was anything to drop.
    void clear();

signals:
    void changed();

private:
    QVector<QVector<BotKeyboardButton>> m_rows;
    bool m_forceReply = false;
};

} // namespace beygram::telegram
