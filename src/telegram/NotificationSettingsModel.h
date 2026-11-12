#pragma once

#include <QObject>

namespace beygram::telegram {

/// The global notification switches of the settings screen: the master toggle,
/// sound and message previews, a quiet-hours window and a "do not disturb"
/// switch.
///
/// The model is a plain value holder. It never reads the clock, never talks to
/// TDLib and performs no I/O, so the caller decides what "now" is and the tests
/// stay deterministic. Every decision is made by shouldNotify(), without
/// logging and without side effects beyond changed().
///
/// Invariants:
///
///  - Quiet hours are expressed in minutes of the day. Every stored bound is
///    clamped to 0..1439, so a caller cannot install a time that does not exist
///    on a clock.
///  - A window whose start equals its end is disabled, not a full day. That is
///    how the settings screen represents "quiet hours off", and it keeps a fresh
///    model silent by default.
///  - A window whose start is later than its end wraps around midnight, so
///    22:00-07:00 covers both the evening and the early morning. The start is
///    inside the window and the end is outside it, which matches the way a user
///    reads "quiet from 22:00 until 07:00".
///  - A minutesOfDay value outside 0..1439 does not name a time of day, so
///    inQuietHours() reports false for it rather than guessing a window. The
///    caller is expected to pass a real clock value.
///  - shouldNotify() refuses as soon as one reason is enough: notifications
///    switched off, do-not-disturb, a muted chat or a quiet-hours window. It
///    answers true only when none of them applies.
///  - changed() is emitted only when a value really differs. Writing the value
///    already stored, or a quiet-hours bound that clamps back to the stored one,
///    is silent, so a refresh that brings the same settings does not repaint the
///    screen or re-trigger a binding loop.
class NotificationSettingsModel final : public QObject {
    Q_OBJECT

public:
    explicit NotificationSettingsModel(QObject* parent = nullptr);

    /// Turns notifications on or off for the whole account. Off means
    /// shouldNotify() refuses everything, whatever the other switches say.
    void setEnabled(bool enabled);

    /// Whether notifications are enabled. They are on by default.
    [[nodiscard]] bool enabled() const;

    /// Turns the notification sound on or off. The flag is reported to the
    /// caller; it does not change whether a notification is shown.
    void setSoundEnabled(bool enabled);

    /// Whether the notification sound is enabled. It is on by default.
    [[nodiscard]] bool soundEnabled() const;

    /// Turns message previews (sender and text in the notification) on or off.
    void setPreviewEnabled(bool enabled);

    /// Whether message previews are enabled. They are on by default.
    [[nodiscard]] bool previewEnabled() const;

    /// Installs the quiet-hours window as minutes of the day. Both bounds are
    /// clamped to 0..1439. Equal bounds disable the window; a start later than
    /// the end wraps around midnight. changed() is emitted only when a stored
    /// bound differs from the clamped value.
    void setQuietHours(int startMinutes, int endMinutes);

    /// The start of the quiet-hours window in minutes of the day, 0..1439.
    [[nodiscard]] int quietStartMinutes() const;

    /// The end of the quiet-hours window in minutes of the day, 0..1439.
    [[nodiscard]] int quietEndMinutes() const;

    /// Whether a quiet-hours window is configured, which is the case exactly
    /// when the start and the end differ.
    [[nodiscard]] bool quietHoursActive() const;

    /// Whether `minutesOfDay` falls inside the quiet-hours window. The start is
    /// included and the end is excluded, so a window is never both active and
    /// over at the same minute. A value outside 0..1439, or a disabled window,
    /// reports false.
    [[nodiscard]] bool inQuietHours(int minutesOfDay) const;

    /// Turns do-not-disturb on or off. While it is on, shouldNotify() refuses
    /// everything regardless of the quiet-hours window.
    void setDnd(bool dnd);

    /// Whether do-not-disturb is on. It is off by default.
    [[nodiscard]] bool dnd() const;

    /// Whether a message that arrived at `minutesOfDay` should raise a
    /// notification, given whether its chat is muted. False when notifications
    /// are disabled, when do-not-disturb is on, when the chat is muted or when
    /// `minutesOfDay` is inside the quiet-hours window.
    [[nodiscard]] bool shouldNotify(int minutesOfDay, bool chatMuted) const;

signals:
    /// Emitted once per real change to any of the settings above.
    void changed();

private:
    bool m_enabled = true;
    bool m_sound = true;
    bool m_preview = true;
    bool m_dnd = false;
    int m_start = 0;
    int m_end = 0;
};

} // namespace beygram::telegram
