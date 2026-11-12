#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {

/// The caption being edited for one media item, e.g. a photo or a video on its
/// way into the composer.
///
/// begin() opens an editor session on the existing caption, trimmed so that
/// surrounding whitespace cannot eat the length budget; an empty caption is a
/// valid starting point. While inactive setText() is ignored, so a stray call
/// from an editor that is already closing cannot rewrite the next caption.
///
/// setText() caps the stored text at 1024 UTF-16 units (Telegram's caption
/// limit) without splitting a surrogate pair, so the text never ends in half of
/// a character. begin() deliberately does not cap: a caption that is already
/// over the limit is kept verbatim rather than silently truncated, which is why
/// remaining() can be negative. canSave() is the single gate for the save
/// action.
///
/// changed() is emitted only when a call actually modifies the model. Caption
/// text is user content and is never logged.
class MediaCaptionModel final : public QObject {
    Q_OBJECT

public:
    explicit MediaCaptionModel(QObject* parent = nullptr);

    /// Activates the editor on original, trimmed. The baseline and the current
    /// text both become that value. Emits changed() only when the session or the
    /// stored text actually differs.
    void begin(const QString& original);

    /// Whether an editor session is open. setText(), dirty(), canSave() and
    /// markSaved() only do anything while this is true.
    [[nodiscard]] bool active() const;

    /// Replaces the caption, capped at 1024 UTF-16 units without splitting a
    /// surrogate pair. Ignored while inactive. Emits changed() only when the
    /// stored text differs.
    void setText(const QString& text);
    [[nodiscard]] QString text() const;

    /// Whether the caption differs from the one begin() was given.
    [[nodiscard]] bool dirty() const;

    /// Stored length in UTF-16 units.
    [[nodiscard]] int length() const;

    /// Units left before the 1024 limit; negative when the caption that begin()
    /// was given was already over it.
    [[nodiscard]] int remaining() const;

    /// active() && dirty() && length() <= 1024.
    [[nodiscard]] bool canSave() const;

    /// Accepts the current text as the new baseline and keeps the editor open.
    /// Emits changed() only when that actually moves the baseline.
    void markSaved();

    /// Closes the editor and clears both the text and the baseline. Emits
    /// changed() only when there was something to clear.
    void cancel();

signals:
    void changed();

private:
    QString m_text;
    QString m_original;
    bool m_active = false;
};

} // namespace beygram::telegram
