#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Holds the draft of an "edit message" session: which chat and message are
/// being edited, the original text the edit started from, and the text the user
/// has typed so far.
///
/// A session starts with begin() and ends with cancel() (the user backed out) or
/// with markSaved() once the edited text has been accepted. The draft is never
/// trimmed here: dirty() compares the exact text, so whitespace-only edits are
/// still reported as changes while canSave() keeps them from being submitted.
///
/// begin() ignores non-positive ids and blank text, and changed() is emitted only
/// when the observed state actually changed.
class MessageEditController final : public QObject {
    Q_OBJECT

public:
    /// Maximum accepted message length, matching the Telegram message limit.
    static constexpr qsizetype kMaxTextLength = 4096;

    explicit MessageEditController(QObject* parent = nullptr);

    /// Starts editing messageId inside chatId with text as both the original and
    /// the initial draft. An edit already in flight is replaced.
    void begin(qint64 chatId, qint64 messageId, const QString& text);

    /// Abandons the edit and clears all state. No-op when inactive.
    void cancel();

    /// True while a valid chat and message are being edited.
    [[nodiscard]] bool active() const;

    /// Chat owning the edited message, or 0 when no edit is active.
    [[nodiscard]] qint64 chatId() const;

    /// Message being edited, or 0 when no edit is active.
    [[nodiscard]] qint64 messageId() const;

    /// Current draft text, or an empty string when no edit is active.
    [[nodiscard]] QString text() const;

    /// Replaces the draft without trimming it. Ignored when no edit is active or
    /// the text is unchanged.
    void setText(const QString& text);

    /// True when the draft differs from the original text.
    [[nodiscard]] bool dirty() const;

    /// True when the draft is active, changed, not blank, and within the limit.
    [[nodiscard]] bool canSave() const;

    /// Accepts the draft as the new original text. No-op when nothing changed.
    void markSaved();

signals:
    /// Emitted whenever the edit state actually changed.
    void changed();

private:
    qint64 m_chatId = 0;
    qint64 m_messageId = 0;
    QString m_text;
    QString m_original;
};

} // namespace beygram::telegram
