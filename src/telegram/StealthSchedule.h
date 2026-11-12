#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {

/// A stealth window as minutes of the day.
///
/// The struct is a plain data holder: StealthSchedule owns the clamping and the
/// rules, so a window read from storage can be handed around without carrying
/// behaviour with it. `enabled` is the user's switch and stays separate from the
/// bounds: a window whose start equals its end is off even while the switch is
/// on.
struct StealthWindow {
    int startMinutes = 0;
    int endMinutes = 0;
    bool enabled = false;
};

/// The daily window during which stealth mode is active, expressed in minutes of
/// the day.
///
/// The model is a plain value holder. It never reads the clock, never talks to
/// TDLib and performs no I/O, so the caller decides what "now" is and the tests
/// stay deterministic. Every decision is made by activeAt() and
/// minutesUntilStart(), without logging and without side effects beyond
/// changed().
///
/// Invariants:
///
///  - Every stored bound is clamped to 0..1439, so a caller cannot install a
///    time that does not exist on a clock.
///  - A window whose start equals its end is off, not a full day. That is how
///    the settings screen spells "no stealth window", and it keeps a fresh
///    schedule inactive by default.
///  - A window whose start is later than its end wraps around midnight, so
///    22:00-08:00 covers both the evening and the early morning. The start is
///    inside the window and the end is outside it, which matches the way a user
///    reads "stealth from 22:00 until 08:00".
///  - The enabled switch is separate: while it is off, no window is active and
///    minutesUntilStart() has nothing to count to.
///  - A minutesOfDay value outside 0..1439 does not name a time of day, so
///    activeAt() reports false and minutesUntilStart() reports -1 rather than
///    guessing a window. The caller is expected to pass a real clock value.
///  - changed() is emitted only when a value really differs. Writing the value
///    already stored, or a bound that clamps back to the stored one, is silent,
///    so a refresh that brings the same schedule does not repaint the screen or
///    re-trigger a binding loop.
///  - reset() restores the defaults (00:00-00:00, off) and stays silent while
///    the schedule already is at them.
class StealthSchedule final : public QObject {
    Q_OBJECT

public:
    explicit StealthSchedule(QObject* parent = nullptr);

    /// Installs the window as minutes of the day. Both bounds are clamped to
    /// 0..1439. The enabled switch is left as it is; equal bounds make the
    /// window off whatever the switch says. changed() is emitted only when a
    /// stored bound differs from the clamped value.
    void setWindow(int startMinutes, int endMinutes);

    /// The start of the window in minutes of the day, 0..1439.
    [[nodiscard]] int startMinutes() const;

    /// The end of the window in minutes of the day, 0..1439.
    [[nodiscard]] int endMinutes() const;

    /// Turns the schedule on or off. While it is off, no window is active and
    /// minutesUntilStart() reports -1.
    void setEnabled(bool enabled);

    /// Whether the schedule is switched on. It is off by default. This is the
    /// user's switch, not whether the window is running right now: activeAt()
    /// answers that, and it is also false while the bounds are equal.
    [[nodiscard]] bool enabled() const;

    /// Whether `minutesOfDay` falls inside the window while the schedule is
    /// enabled. The start is included and the end is excluded, so a window is
    /// never both active and over at the same minute. A value outside 0..1439,
    /// a disabled schedule or a window whose bounds are equal reports false.
    [[nodiscard]] bool activeAt(int minutesOfDay) const;

    /// Whole minutes from `minutesOfDay` to the next start of the window, in
    /// 1..1439. Reports -1 while the window is active, when the schedule is off,
    /// when the bounds are equal or when `minutesOfDay` is outside 0..1439.
    [[nodiscard]] int minutesUntilStart(int minutesOfDay) const;

    /// The window for the settings screen: "22:00 - 08:00" (with an en dash
    /// between the bounds), or "Off" while no window is configured, which is
    /// also what equal bounds or a disabled schedule show.
    [[nodiscard]] QString windowText() const;

    /// Restores the defaults: 00:00-00:00 and off. Emits changed() once if
    /// anything actually moved, and stays silent when the schedule already is at
    /// its defaults.
    void reset();

signals:
    /// Emitted once per real change to any of the values above, and once for a
    /// reset() that actually restored something.
    void changed();

private:
    [[nodiscard]] bool isConfigured() const;

    StealthWindow m_window;
};

} // namespace beygram::telegram
