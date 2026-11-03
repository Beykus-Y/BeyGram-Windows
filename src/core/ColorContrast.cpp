#include "ColorContrast.h"

#include <algorithm>
#include <cmath>

namespace beygram::color {
namespace {

double linearize(double channel)
{
    return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
}

} // namespace

double relativeLuminance(const QColor& color)
{
    const QColor rgb = color.toRgb();
    const double r = linearize(rgb.redF());
    const double g = linearize(rgb.greenF());
    const double b = linearize(rgb.blueF());
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

double contrastRatio(const QColor& foreground, const QColor& background)
{
    const double a = relativeLuminance(foreground);
    const double b = relativeLuminance(background);
    const double lighter = std::max(a, b);
    const double darker = std::min(a, b);
    return (lighter + 0.05) / (darker + 0.05);
}

bool isReadableText(const QColor& foreground, const QColor& background, double minimum)
{
    return contrastRatio(foreground, background) >= minimum;
}

QColor readableOn(const QColor& background, const QColor& preferred, const QColor& fallback)
{
    if (isReadableText(preferred, background))
        return preferred;
    if (isReadableText(fallback, background))
        return fallback;
    return relativeLuminance(background) > 0.5 ? QColor(Qt::black) : QColor(Qt::white);
}

QColor mix(const QColor& from, const QColor& to, double amount)
{
    const double t = std::clamp(amount, 0.0, 1.0);
    const QColor a = from.toRgb();
    const QColor b = to.toRgb();
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

} // namespace beygram::color
