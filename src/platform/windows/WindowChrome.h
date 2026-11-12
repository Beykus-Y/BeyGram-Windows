#pragma once

#include <QColor>

class QWindow;

namespace beygram::platform {

/// Synchronizes a top-level Win32 window's DWM title bar with the active
/// BeyGram theme (immersive dark frame on Windows 10/11 and matching caption
/// and text colors on Windows 11 22H2+). Safe no-op on unsupported OS builds.
void applyWindowTitleBarTheme(QWindow* window, bool dark, const QColor& captionColor,
                              const QColor& textColor);

} // namespace beygram::platform
