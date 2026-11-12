#include "WindowChrome.h"

#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#endif

namespace beygram::platform {

void applyWindowTitleBarTheme(QWindow* window, bool dark, const QColor& captionColor,
                              const QColor& textColor)
{
#ifdef Q_OS_WIN
    if (!window)
        return;

    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd)
        return;

    const BOOL darkValue = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkValue, sizeof(darkValue));

    if (captionColor.isValid()) {
        const COLORREF captionRef =
            RGB(captionColor.red(), captionColor.green(), captionColor.blue());
        DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &captionRef, sizeof(captionRef));
    }
    if (textColor.isValid()) {
        const COLORREF textRef = RGB(textColor.red(), textColor.green(), textColor.blue());
        DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &textRef, sizeof(textRef));
    }
#else
    Q_UNUSED(window)
    Q_UNUSED(dark)
    Q_UNUSED(captionColor)
    Q_UNUSED(textColor)
#endif
}

} // namespace beygram::platform
