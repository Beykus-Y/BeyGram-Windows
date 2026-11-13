#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// What one chat row shows besides the title: the last-message preview line,
/// the transient typing marker, the unread badge and the row's flags.
///
/// This is a plain snapshot, not a normalised record. ChatPreviewModel
/// collapses `text` to one line of at most 200 characters and clamps
/// `unreadCount` to zero, so a value read back from the model can differ from
/// the one handed in whenever it was dirty.
struct ChatPreviewState {
    QString text;
    bool isTyping = false;
    int unreadCount = 0;
    bool muted = false;
    bool pinned = false;
    bool draft = false;
};

/// The preview state of every chat, keyed by chat id, plus the date of each
/// chat's last message.
///
/// TDLib sends the chat list as a full snapshot; the transport feeds each row
/// here through setPreview() and a view repaints on changed(). The message date
/// lives next to the preview because a row paints both, but it has its own
/// setter: a chat can have a date before its preview arrives, and setPreview()
/// must not drop that date.
///
/// Normalisation happens on the way in: the text is collapsed to a single line
/// and capped at 200 UTF-16 units without splitting a surrogate pair, and the
/// unread count is clamped to zero. A state with nothing to paint (empty text,
/// no flags, no unread count) is not stored, so preview() of such a chat is the
/// default state. A chat whose only fact is its last-message date keeps an
/// entry with an empty state.
///
/// changed() is emitted only when a stored entry really differs. Ignored chat
/// ids (not positive), identical previews, clearing a date that was absent and
/// remove()/clear() on nothing are all silent.
///
/// The model keeps no clock: timeText() takes `nowUnix` from the caller, which
/// keeps tests deterministic. It sends nothing to TDLib and never logs.
class ChatPreviewModel final : public QObject {
    Q_OBJECT

public:
    explicit ChatPreviewModel(QObject* parent = nullptr);

    /// Replaces the preview of `chatId` and keeps its last-message date. The
    /// text is collapsed to one line and capped at 200 characters, and a
    /// negative unread count becomes zero. A state with nothing to paint
    /// clears the preview. Ignored when the chat id is not positive.
    void setPreview(qint64 chatId, const ChatPreviewState& state);

    /// The stored preview of `chatId`, or a default state when the chat has
    /// none. The returned text is already normalised.
    [[nodiscard]] ChatPreviewState preview(qint64 chatId) const;

    /// The unread badge of `chatId`: empty when nothing is unread, "•" for a
    /// muted chat with unread messages, the count itself up to `maxShown`, and
    /// "maxShown+" above that. `maxShown` is clamped to at least one, so a
    /// badge never reads "0+".
    [[nodiscard]] QString badgeText(qint64 chatId, int maxShown = 99) const;

    /// The age label of the chat's last message relative to `nowUnix`: "now"
    /// under a minute, "Nm" under an hour, "Nh" under a day, "Yesterday" under
    /// two days, and "dd.MM" in local time after that. A date in the future,
    /// e.g. from clock skew, reads as "now". Empty when the chat has no date.
    [[nodiscard]] QString timeText(qint64 chatId, qint64 nowUnix) const;

    /// Sets the Unix date of the chat's last message. A non-positive value
    /// clears the date and leaves the preview alone. Ignored when the chat id
    /// is not positive.
    void setLastMessageDate(qint64 chatId, qint64 date);

    /// Forgets the preview and the date of `chatId`. Silent when the chat has
    /// nothing stored.
    void remove(qint64 chatId);

    /// Forgets every chat. Silent when the model is already empty.
    void clear();

signals:
    /// Emitted when a stored preview or last-message date really changed.
    void changed();

private:
    struct Entry {
        ChatPreviewState state;
        qint64 lastMessageDate = 0;
    };

    /// Writes `entry` for `chatId`, erasing it when it has nothing to paint,
    /// and reports whether the store really moved.
    bool storeEntry(qint64 chatId, const Entry& entry);

    QHash<qint64, Entry> m_entries;
};

} // namespace beygram::telegram
