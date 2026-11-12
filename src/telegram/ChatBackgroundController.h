#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// What the chat area paints behind the messages.
enum class BackgroundKind {
    None,      ///< Plain theme colour, no background asset.
    Solid,     ///< One colour, e.g. "#1b2836".
    Gradient,  ///< A gradient description understood by the view.
    Pattern,   ///< A repeating tile.
    Wallpaper, ///< A full-bleed image.
};

/// The background state of one chat, or of every chat when no chat is selected.
///
/// The controller only stores and validates the choice; it never loads a file
/// or talks to TDLib. A view binds its pickers to the getters, calls the setters
/// from its controls, and repaints on changed().
///
/// The invariant is that value() is only non-empty for a real background:
/// selecting None clears it, and a non-None kind is rejected while the value is
/// blank, so the stored state is always renderable.
///
/// chatId() 0 means "global/default background", the value used by chats that
/// have no override of their own. Negative ids are real (groups, channels), so
/// they are accepted like any other id.
///
/// isDefault() describes the look only: None with dimmer and blur at 0. A
/// global or per-chat scope does not affect it.
class ChatBackgroundController final : public QObject {
    Q_OBJECT

public:
    explicit ChatBackgroundController(QObject* parent = nullptr);

    /// Selects `kind` with `value`. None clears the stored value and ignores
    /// `value`. Any other kind needs a non-blank value; a blank one is rejected
    /// and the previous background is kept, because a kind without a value
    /// cannot be rendered.
    void setBackground(BackgroundKind kind, const QString& value);

    [[nodiscard]] BackgroundKind kind() const { return m_kind; }
    [[nodiscard]] QString value() const { return m_value; }

    /// How much the background is darkened behind the messages, in percent.
    /// Values outside 0..100 are clamped instead of rejected.
    void setDimmer(int percent);
    [[nodiscard]] int dimmer() const { return m_dimmer; }

    /// How much the background is blurred, in percent. Clamped to 0..100.
    void setBlur(int percent);
    [[nodiscard]] int blur() const { return m_blur; }

    /// Selects the chat this background applies to. 0 means the global
    /// background; negative ids are valid chats. This only changes the scope,
    /// it never resets the look.
    void setForChat(qint64 chatId);
    [[nodiscard]] qint64 chatId() const { return m_chatId; }

    /// Restores the defaults: no background, no dimmer, no blur, and back to the
    /// global scope. Emits changed() once if anything actually moved.
    void reset();

    /// True while the look is untouched: None with dimmer and blur both 0.
    [[nodiscard]] bool isDefault() const;

signals:
    /// Emitted whenever the background, dimmer, blur or chat scope really
    /// changed. Rejected values and no-op setters stay silent.
    void changed();

private:
    BackgroundKind m_kind = BackgroundKind::None;
    QString m_value;
    int m_dimmer = 0;
    int m_blur = 0;
    qint64 m_chatId = 0;
};

} // namespace beygram::telegram
