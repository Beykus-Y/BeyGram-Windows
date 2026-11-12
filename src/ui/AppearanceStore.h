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
    QString themePreset = QStringLiteral("Warm");        // Warm | Graphite
    QString themeMode = QStringLiteral("FollowSystem");  // FollowSystem | Light | Dark | Amoled
    QString designStyle = QStringLiteral("Default");     // Default | Material2 | Material3 | LiquidGlass | Signature
    QString bubbleStyle = QStringLiteral("Rounded");     // Rounded | Soft | Square
    QString wallpaperPreset = QStringLiteral("none");    // none | lavender-flow | ...
    int wallpaperIntensity = 24;                         // 0..45
    bool reduceMotion = false;
    QString listDensity = QStringLiteral("Comfortable"); // Comfortable | Compact
    int messageTextSize = 14;                            // 12..18
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
    Q_PROPERTY(QString themePreset READ themePreset WRITE setThemePreset NOTIFY changed)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY changed)
    Q_PROPERTY(QString designStyle READ designStyle WRITE setDesignStyle NOTIFY changed)
    Q_PROPERTY(QString bubbleStyle READ bubbleStyle WRITE setBubbleStyle NOTIFY changed)
    Q_PROPERTY(QString wallpaperPreset READ wallpaperPreset WRITE setWallpaperPreset NOTIFY changed)
    Q_PROPERTY(int wallpaperIntensity READ wallpaperIntensity WRITE setWallpaperIntensity NOTIFY changed)
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY changed)
    Q_PROPERTY(QString listDensity READ listDensity WRITE setListDensity NOTIFY changed)
    Q_PROPERTY(int messageTextSize READ messageTextSize WRITE setMessageTextSize NOTIFY changed)

public:
    explicit AppearanceStore(QString filePath, QObject* parent = nullptr);

    /// Replaces the in-memory settings with the file contents. Emits changed()
    /// once if that actually altered anything; never writes the file.
    void load();
    [[nodiscard]] AppearanceSettings settings() const;
    [[nodiscard]] QString filePath() const;

    [[nodiscard]] QString themePreset() const { return m_settings.themePreset; }
    [[nodiscard]] QString themeMode() const { return m_settings.themeMode; }
    [[nodiscard]] QString designStyle() const { return m_settings.designStyle; }
    [[nodiscard]] QString bubbleStyle() const { return m_settings.bubbleStyle; }
    [[nodiscard]] QString wallpaperPreset() const { return m_settings.wallpaperPreset; }
    [[nodiscard]] int wallpaperIntensity() const { return m_settings.wallpaperIntensity; }
    [[nodiscard]] bool reduceMotion() const { return m_settings.reduceMotion; }
    [[nodiscard]] QString listDensity() const { return m_settings.listDensity; }
    [[nodiscard]] int messageTextSize() const { return m_settings.messageTextSize; }

    Q_INVOKABLE void setThemePreset(const QString& value);
    Q_INVOKABLE void setThemeMode(const QString& value);
    Q_INVOKABLE void setDesignStyle(const QString& value);
    Q_INVOKABLE void setBubbleStyle(const QString& value);
    Q_INVOKABLE void setWallpaperPreset(const QString& value);
    Q_INVOKABLE void setWallpaperIntensity(int value);
    Q_INVOKABLE void setReduceMotion(bool value);
    Q_INVOKABLE void setListDensity(const QString& value);
    Q_INVOKABLE void setMessageTextSize(int value);

signals:
    void changed();

private:
    void persist();
    QString m_filePath;
    AppearanceSettings m_settings;
};

} // namespace beygram::ui
