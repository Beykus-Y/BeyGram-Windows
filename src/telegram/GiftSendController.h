#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Holds the form state for sending one catalog gift and turns it into a TDLib
/// sendGift request through sendGiftRequest() from GiftRequests.h.
///
/// Nothing is assembled here: GiftRequests.h stays the single builder, so the
/// schema shape (owner_id as messageSenderUser, text as a formattedText with no
/// entities, pay_for_upgrade false) is defined in one place. The mapping is:
///
///   giftId    -> sendGift.gift_id
///   chatId    -> sendGift.owner_id (messageSenderUser.user_id). TDLib keeps a
///                private chat id equal to its user id, which is the convention
///                sendGiftRequest() documents.
///   userId    -> the same owner_id, used when no positive chatId is known yet
///                (the recipient has been picked as a user but its chat has not
///                been opened). A positive chatId always wins over userId.
///   message   -> sendGift.text, sent verbatim as formattedText.
///   anonymous -> sendGift.is_private. The builder fixes that field to false,
///                so request() flips exactly that one field for an anonymous
///                send; pay_for_upgrade stays false either way.
///   starCount -> not part of the request. sendGift prices the gift from its
///                catalog gift_id, so the star count is only shown to the user
///                and checked by valid().
///
/// A gift cannot be sent to a group or a channel (that needs a
/// messageSenderChat owner, which the builder cannot express), so valid()
/// requires a positive recipient id even though a negative chat id is stored as
/// given. valid() also requires a positive gift id and a positive star count,
/// because a free gift does not exist in the catalog.
///
/// The setters emit changed() only when the stored state actually changed.
/// starCount is clamped to zero when negative, and the message is trimmed and
/// capped at 255 characters; the recipient and gift ids are stored as given and
/// judged by valid(). The cap counts a surrogate pair as one character and never
/// splits one, so an emoji is kept whole. The real server limit is
/// getOption("gift_text_length_max"), which the builder refuses to guess; 255 is
/// the local cap the form uses.
class GiftSendController final : public QObject {
    Q_OBJECT

public:
    explicit GiftSendController(QObject* parent = nullptr);

    /// Sets the recipient. A positive chatId is the private chat that receives
    /// the gift; userId is the fallback owner when chatId is not positive. Ids
    /// are stored as given, so a group/channel id stays visible in chatId() but
    /// can never make valid() true.
    void setRecipient(qint64 chatId, qint64 userId);

    /// The recipient chat id, or zero when none is set.
    [[nodiscard]] qint64 chatId() const;

    /// The recipient user id, or zero when none is set.
    [[nodiscard]] qint64 userId() const;

    /// Sets the chosen catalog gift and its price in stars. A negative
    /// starCount is clamped to zero (no gift is free); giftId is stored as
    /// given and valid() rejects a non-positive one.
    void setGift(qint64 giftId, qint64 starCount);

    /// The chosen gift id, or zero when none is set.
    [[nodiscard]] qint64 giftId() const;

    /// The gift price in stars, never negative.
    [[nodiscard]] qint64 starCount() const;

    /// Sets the text sent with the gift. The value is trimmed and capped at 255
    /// characters, so trailing spaces and a whitespace-only message normalise to
    /// an empty message.
    void setMessage(const QString& text);

    /// The normalised message, or an empty string when none is set.
    [[nodiscard]] QString message() const;

    /// Selects an anonymous send (sendGift.is_private).
    void setAnonymous(bool anonymous);

    /// True when the gift is sent anonymously.
    [[nodiscard]] bool anonymous() const;

    /// True when the state can be sent: a positive gift id, a positive star
    /// count and a positive chat or user id.
    [[nodiscard]] bool valid() const;

    /// Builds the sendGift request for the current state via sendGiftRequest().
    /// Returns an empty object when valid() is false.
    [[nodiscard]] QJsonObject request() const;

    /// Clears the whole form. Does nothing, and emits nothing, when the form is
    /// already empty.
    void reset();

signals:
    /// Emitted whenever the stored form state actually changed.
    void changed();

private:
    qint64 m_chatId = 0;
    qint64 m_userId = 0;
    qint64 m_giftId = 0;
    qint64 m_stars = 0;
    QString m_message;
    bool m_anonymous = false;
};

} // namespace beygram::telegram
