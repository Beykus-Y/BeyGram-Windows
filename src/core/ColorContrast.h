#pragma once

#include <QColor>
#include <QString>

namespace beygram::color {

/// WCAG 2.1 relative luminance of an sRGB colour.
[[nodiscard]] double relativeLuminance(const QColor& color);

/// WCAG 2.1 contrast ratio (1.0 .. 21.0).
[[nodiscard]] double contrastRatio(const QColor& foreground, const QColor& background);

/// The BeyGram Theme Engine rule: body text must reach 4.5:1.
constexpr double MinimumTextContrast = 4.5;
/// Accents and large UI strokes must reach 3:1.
constexpr double MinimumAccentContrast = 3.0;

[[nodiscard]] bool isReadableText(const QColor& foreground, const QColor& background,
                                  double minimum = MinimumTextContrast);

/// Returns a legible variant of `preferred` on `background`. When the preferred
/// colour fails, the theme's `fallback` is used; if that fails too, black or
/// white is chosen by the background's luminance. This mirrors the Android
/// Theme Engine's "a stored pick that no longer reads falls back" rule.
[[nodiscard]] QColor readableOn(const QColor& background, const QColor& preferred,
                                const QColor& fallback);

/// Blend two colours (alpha-aware, non-premultiplied), used by the QML palette.
[[nodiscard]] QColor mix(const QColor& from, const QColor& to, double amount);

} // namespace beygram::color
