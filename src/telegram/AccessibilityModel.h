#pragma once

#include <QObject>

namespace beygram::telegram {

/// The accessibility section of the settings screen: reduce motion, high
/// contrast, large text, a text scale and screen-reader hints.
///
/// The model is a plain value holder. It never talks to TDLib, never reads the
/// clock and performs no I/O, so a view binds its controls to the getters, calls
/// the setters and repaints on changed(). Logging and persistence are the
/// caller's job.
///
/// Invariants:
///
///  - Defaults match a user who never touched the accessibility settings: every
///    switch off and the text scale at 100 percent.
///  - The switches are independent: writing one never touches another. In
///    particular, largeText is not derived from textScale and does not force the
///    scale up, because the two are separate user choices.
///  - textScale is clamped to the inclusive range 85..150. A value outside the
///    range is stored clamped, so the getter never reports it.
///  - changed() is emitted only when a value really differs, including when a
///    clamped request lands on the value already stored.
///  - reset() is silent while the model already is at its defaults and emits
///    changed() exactly once when it actually restores something.
///  - isDefault() is true exactly when every switch is off and the scale is 100,
///    which is the state reset() installs.
class AccessibilityModel final : public QObject {
    Q_OBJECT

public:
    explicit AccessibilityModel(QObject* parent = nullptr);

    /// Whether animations should be reduced. Off by default.
    void setReduceMotion(bool on);
    [[nodiscard]] bool reduceMotion() const;

    /// Whether the high-contrast palette should be used. Off by default.
    void setHighContrast(bool on);
    [[nodiscard]] bool highContrast() const;

    /// Whether text should use the large-text presentation. Off by default and
    /// independent of textScale().
    void setLargeText(bool on);
    [[nodiscard]] bool largeText() const;

    /// Text scale in percent, clamped to 85..150. 100 by default.
    void setTextScale(int percent);
    [[nodiscard]] int textScale() const;

    /// Whether screen-reader hints should be exposed. Off by default.
    void setScreenReaderHints(bool on);
    [[nodiscard]] bool screenReaderHints() const;

    /// Restores the defaults: all switches off and the scale at 100. Emits
    /// changed() once if anything actually moved, and stays silent when the
    /// model is already at its defaults.
    void reset();

    /// True while the model equals its defaults, which is the state reset()
    /// installs.
    [[nodiscard]] bool isDefault() const;

signals:
    /// Emitted once per real change to any of the values above, and once for a
    /// reset() that actually restored something.
    void changed();

private:
    bool m_motion = false;
    bool m_contrast = false;
    bool m_large = false;
    bool m_hints = false;
    int m_scale = 100;
};

} // namespace beygram::telegram
