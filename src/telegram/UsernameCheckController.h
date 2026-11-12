#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {

/// Tracks one username being checked for availability: the normalised text the
/// user typed and where that text stands on the way to an answer.
///
/// setInput() is the only entry point for text. It strips a pasted leading '@',
/// trims and lower cases, so the controller always holds the canonical form a
/// checkUsername request would carry, and it decides the status in one step:
/// empty text is Idle, malformed text is Invalid and must not produce a
/// request, and a well-formed name is Checking because a request is now due.
///
/// setResult() answers that request and is accepted only while Checking, so a
/// late answer for a name the user has already edited away is dropped instead
/// of being shown against the new name. reset() abandons everything.
///
/// changed() is emitted only when the input or the status really changed, so a
/// view bound to it does not repaint on repeated keystrokes.
class UsernameCheckController final : public QObject {
    Q_OBJECT

public:
    /// Where one username stands on the way from typed text to a yes/no answer.
    /// Idle means nothing is typed, Invalid text is not a possible username and
    /// was never sent, Checking a well-formed name is waiting for its answer,
    /// and Available/Taken are that answer.
    enum class Status { Idle, Invalid, Checking, Available, Taken };

    explicit UsernameCheckController(QObject* parent = nullptr);

    /// Records the text the user typed. A leading '@' (or several, as pasted
    /// from a t.me link), surrounding whitespace and upper case are removed, so
    /// " @Alice_2024 " and "alice_2024" are the same username.
    ///
    /// Text that is empty after normalisation is Idle, malformed text is
    /// Invalid, and a well-formed name is Checking. Repeating the same
    /// canonical text is a no-op: a pending request or a known answer already
    /// belongs to it, so typing one more character and deleting it again cannot
    /// throw the answer away. Call reset() first to force a fresh check.
    void setInput(const QString& input);

    /// The canonical text, i.e. what setInput() would store for the same input.
    [[nodiscard]] QString input() const;

    [[nodiscard]] Status status() const;

    /// True only for Available; Invalid, Checking and Taken all block sending.
    [[nodiscard]] bool canSubmit() const;

    /// Records the answer to the pending check. Ignored unless the status is
    /// Checking, so an answer that arrives after the user moved on is dropped
    /// instead of being shown for the new name.
    void setResult(bool available);

    /// Drops the text and the status back to Idle. Silent when already there.
    void reset();

    /// True for 5 to 32 characters from [a-z0-9_] that start with a letter.
    /// This is the canonical form, so callers strip '@' and lower case first;
    /// setInput() does that for them.
    [[nodiscard]] static bool validFormat(const QString& username);

signals:
    /// Emitted whenever the input or the status actually changed.
    void changed();

private:
    QString m_input;
    Status m_status = Status::Idle;
};

} // namespace beygram::telegram
