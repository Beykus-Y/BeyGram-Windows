#pragma once

#include <QColor>
#include <QObject>
#include <QString>

class QStyleHints;

namespace beygram::ui {

/// Colour tokens ported from the Android Theme Engine
/// (`ui/theme/Color.kt`). Phase 0 ships the Warm and Graphite presets plus the
/// Same brightness rules (FollowSystem / Light / Dark / Amoled). MaterialYou
/// (wallpaper colours) stays an Android-only capability until a Windows
/// equivalent is designed.
class ThemeController final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QColor bg READ bg NOTIFY themeChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY themeChanged)
    Q_PROPERTY(QColor surface2 READ surface2 NOTIFY themeChanged)
    Q_PROPERTY(QColor line READ line NOTIFY themeChanged)
    Q_PROPERTY(QColor text READ text NOTIFY themeChanged)
    Q_PROPERTY(QColor muted READ muted NOTIFY themeChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QColor accentSoft READ accentSoft NOTIFY themeChanged)
    Q_PROPERTY(QColor accentInk READ accentInk NOTIFY themeChanged)
    Q_PROPERTY(QColor onAccent READ onAccent NOTIFY themeChanged)
    Q_PROPERTY(QColor bubbleOut READ bubbleOut NOTIFY themeChanged)
    Q_PROPERTY(QColor ok READ ok NOTIFY themeChanged)
    Q_PROPERTY(QColor okInk READ okInk NOTIFY themeChanged)
    Q_PROPERTY(QColor warn READ warn NOTIFY themeChanged)
    Q_PROPERTY(QColor warnInk READ warnInk NOTIFY themeChanged)
    Q_PROPERTY(QColor danger READ danger NOTIFY themeChanged)

    Q_PROPERTY(bool darkMode READ darkMode NOTIFY themeChanged)
    Q_PROPERTY(QString modeName READ modeName WRITE setModeName NOTIFY themeChanged)
    Q_PROPERTY(QString presetName READ presetName WRITE setPresetName NOTIFY themeChanged)

public:
    explicit ThemeController(QObject* parent = nullptr);
    ~ThemeController() override;

    [[nodiscard]] QColor bg() const { return m_bg; }
    [[nodiscard]] QColor surface() const { return m_surface; }
    [[nodiscard]] QColor surface2() const { return m_surface2; }
    [[nodiscard]] QColor line() const { return m_line; }
    [[nodiscard]] QColor text() const { return m_text; }
    [[nodiscard]] QColor muted() const { return m_muted; }
    [[nodiscard]] QColor accent() const { return m_accent; }
    [[nodiscard]] QColor accentSoft() const { return m_accentSoft; }
    [[nodiscard]] QColor accentInk() const { return m_accentInk; }
    [[nodiscard]] QColor onAccent() const { return m_onAccent; }
    [[nodiscard]] QColor bubbleOut() const { return m_bubbleOut; }
    [[nodiscard]] QColor ok() const { return m_ok; }
    [[nodiscard]] QColor okInk() const { return m_okInk; }
    [[nodiscard]] QColor warn() const { return m_warn; }
    [[nodiscard]] QColor warnInk() const { return m_warnInk; }
    [[nodiscard]] QColor danger() const { return m_danger; }

    [[nodiscard]] bool darkMode() const { return m_dark; }
    [[nodiscard]] QString modeName() const;
    [[nodiscard]] QString presetName() const;
    void setModeName(const QString& name);
    void setPresetName(const QString& name);

    Q_INVOKABLE void toggleDarkMode();

signals:
    void themeChanged();

private:
    enum class Mode { FollowSystem, Light, Dark, Amoled };
    enum class Preset { Warm, Graphite };

    void apply();
    [[nodiscard]] bool systemIsDark() const;

    Mode m_mode = Mode::FollowSystem;
    Preset m_preset = Preset::Warm;
    QStyleHints* m_styleHints = nullptr;
    bool m_dark = false;

    QColor m_bg;
    QColor m_surface;
    QColor m_surface2;
    QColor m_line;
    QColor m_text;
    QColor m_muted;
    QColor m_accent;
    QColor m_accentSoft;
    QColor m_accentInk;
    QColor m_onAccent;
    QColor m_bubbleOut;
    QColor m_ok;
    QColor m_okInk;
    QColor m_warn;
    QColor m_warnInk;
    QColor m_danger;
};

} // namespace beygram::ui
