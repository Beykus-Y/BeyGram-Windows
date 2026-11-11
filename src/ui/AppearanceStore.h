#pragma once

#include <QObject>
#include <QString>

namespace beygram::ui {

/// The persisted appearance choices, mirroring Android `data/appearance/`.
///
/// Every value is stored by the name Android writes (`ThemePreset`, `ThemeMode`,
/// `DesignStyle`, `BubbleStyle` and the `WallpaperPresets` ids), so a profile
/// reads the same on both clients.
///
/// `themePreset` accepts only the presets the Windows theme engine can render
/// (`Warm`, `Graphite`). Android's `MaterialYou` derives its colours from the
/// system wallpaper and stays an Android-only capability, so it is rejected here
/// and falls back to `Warm`, exactly like an unknown value.
struct AppearanceSettings {
    QString themePreset = QStringLiteral("Warm");      // Warm | Graphite
    QString themeMode = QStringLiteral("FollowSystem"); // FollowSystem | Light | Dark | Amoled
    QString designStyle = QStringLiteral("Default");    // Default | Material2 | Material3 | LiquidGlass | Signature
    QString bubbleStyle = QStringLiteral("Rounded");    // Rounded | Soft | Square
    QString wallpaperPreset = QStringLiteral("none");   // none | lavender-flow | ...
    int wallpaperIntensity = 24;                        // 0..45
    bool reduceMotion = false;
};

/// JSON-backed appearance settings.
///
/// The file is `{"version":1,...}`. Missing, damaged or unknown values fall back
/// to the defaults above, so a corrupt file can never make the client unusable.
/// Setters validate, clamp and only write (and emit changed()) when a value
/// actually changes. Writes go through QSaveFile, so a crash mid-write keeps the
/// previous file intact. Nothing about the file contents is logged.
class AppearanceStore final : public QObject {
    Q_OBJECT

public:
    explicit AppearanceStore(QString filePath, QObject* parent = nullptr);

    /// Replaces the in-memory settings with the file contents. Emits changed()
    /// once if that actually altered anything; never writes the file.
    void load();
    [[nodiscard]] AppearanceSettings settings() const;
    [[nodiscard]] QString filePath() const;

    void setThemePreset(const QString& value);
    void setThemeMode(const QString& value);
    void setDesignStyle(const QString& value);
    void setBubbleStyle(const QString& value);
    void setWallpaperPreset(const QString& value);
    void setWallpaperIntensity(int value);
    void setReduceMotion(bool value);

signals:
    void changed();

private:
    void persist();
    QString m_filePath;
    AppearanceSettings m_settings;
};

} // namespace beygram::ui
