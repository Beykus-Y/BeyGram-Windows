#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {

/// The action a horizontal swipe on a chat row performs.
///
/// None leaves the gesture inert, so a direction can be switched off without
/// hiding the gesture settings. The enumerator order is an implementation
/// detail: settings are persisted through actionName()/actionFromName(), never
/// through the numeric value.
enum class SwipeAction {
    None,
    Archive,
    Mute,
    Read,
    Pin,
    Delete,
};

/// One gesture assignment per swipe direction.
///
/// The two sides are always different, because a chat row cannot archive on
/// both a left and a right swipe. ChatSwipeSettingsModel keeps that invariant,
/// so a view can bind two pickers without checking for collisions itself.
struct SwipeSettings {
    SwipeAction left = SwipeAction::Archive;
    SwipeAction right = SwipeAction::Read;

    bool operator==(const SwipeSettings& other) const = default;
};

/// The swipe gesture assignments of the chat list.
///
/// The model is a plain value holder: it never talks to TDLib, never reads the
/// clock and performs no I/O, so a view binds its two pickers to settings(),
/// calls setLeft()/setRight() from them and repaints on changed(). Reading and
/// writing the stored pair is the caller's job.
///
/// Invariants:
///
///  - left and right always differ. Assigning a side the action the other side
///    already holds swaps the two instead of duplicating it, so the gesture the
///    user picked stays available in the opposite direction.
///  - Assigning the action a side already holds is a no-op and does not emit.
///  - setSettings() installs a whole pair at once. A pair whose two sides are
///    equal is repaired: the requested left is kept and the right becomes the
///    default right, or the default left when the requested left already is the
///    default right. A corrupt stored value therefore cannot break the
///    invariant.
///  - changed() is emitted at most once per call and only when a value really
///    differs, so a refresh that brings the same pair does not repaint the
///    screen or re-trigger a binding loop.
///  - reset() restores the defaults, is silent while the model already is at
///    them and emits changed() exactly once otherwise.
///  - isDefault() is true exactly for the default pair, Archive on the left and
///    Read on the right.
class ChatSwipeSettingsModel final : public QObject {
    Q_OBJECT

public:
    explicit ChatSwipeSettingsModel(QObject* parent = nullptr);

    /// Installs both sides at once, repairing a pair whose sides are equal as
    /// described above. Emits changed() once when the stored pair differs.
    void setSettings(const SwipeSettings& settings);
    [[nodiscard]] SwipeSettings settings() const;

    /// Assigns the left gesture, swapping the sides when the action is the one
    /// the right side already holds. Returns true when the model changed, and
    /// emits changed() at most once.
    bool setLeft(SwipeAction action);

    /// Assigns the right gesture, with the same swap and signal rules as
    /// setLeft().
    bool setRight(SwipeAction action);

    /// The stored name of the action, lowercase: none, archive, mute, read,
    /// pin or delete. A value outside the enumeration reads as "none".
    [[nodiscard]] QString actionName(SwipeAction action) const;

    /// Parses a stored name. Matching is case-sensitive; an unknown name, an
    /// empty string included, is None.
    [[nodiscard]] SwipeAction actionFromName(const QString& name) const;

    /// True while the pair is the default one: Archive left, Read right.
    [[nodiscard]] bool isDefault() const;

    /// Restores the default pair. Emits changed() once if anything moved, and
    /// stays silent when the model already is at its defaults.
    void reset();

signals:
    /// Emitted once per real change to either side.
    void changed();

private:
    SwipeSettings m_settings;
};

} // namespace beygram::telegram
