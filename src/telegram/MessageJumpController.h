#pragma once

#include <QObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Tracks an in-flight "jump to message" navigation: which chat and message the
/// view is scrolling to, and which message is currently highlighted once it has
/// arrived.
///
/// A jump starts with begin() and ends with either finish() (the message was
/// reached) or cancel() (the user navigated away). While no jump is active the
/// highlight is empty, so setHighlight() is ignored and the view cannot paint a
/// stale highlight for a message from a previous jump.
///
/// Non-positive chat and message ids are ignored, and changed() is emitted only
/// when the observed state actually changed.
class MessageJumpController final : public QObject {
    Q_OBJECT

public:
    explicit MessageJumpController(QObject* parent = nullptr);

    /// Starts a jump to messageId inside chatId. A jump already in flight is
    /// replaced, and the target itself becomes the initial highlight.
    void begin(qint64 chatId, qint64 messageId);

    /// True while a valid chat and target are set.
    [[nodiscard]] bool active() const;

    /// Chat owning the jump, or 0 when no jump is active.
    [[nodiscard]] qint64 chatId() const;

    /// Message being scrolled to, or 0 when no jump is active.
    [[nodiscard]] qint64 targetId() const;

    /// Message currently highlighted, or 0 when no jump is active.
    [[nodiscard]] qint64 highlightId() const;

    /// Moves the highlight to messageId. Ignored when no jump is active or the
    /// id is not positive.
    void setHighlight(qint64 messageId);

    /// Completes the jump and clears all state. No-op when inactive.
    void finish();

    /// Aborts the jump and clears all state. No-op when inactive.
    void cancel();

signals:
    /// Emitted whenever the jump state actually changed.
    void changed();

private:
    qint64 m_chatId = 0;
    qint64 m_target = 0;
    qint64 m_highlight = 0;
};

} // namespace beygram::telegram
