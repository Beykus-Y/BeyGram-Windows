#include "AccessibilityModel.h"

#include <QtGlobal>

namespace beygram::telegram {

namespace {

constexpr int kMinTextScale = 85;
constexpr int kMaxTextScale = 150;
constexpr int kDefaultTextScale = 100;

} // namespace

AccessibilityModel::AccessibilityModel(QObject* parent)
    : QObject(parent)
{
}

void AccessibilityModel::setReduceMotion(bool on)
{
    if (m_motion == on)
        return;
    m_motion = on;
    emit changed();
}

bool AccessibilityModel::reduceMotion() const
{
    return m_motion;
}

void AccessibilityModel::setHighContrast(bool on)
{
    if (m_contrast == on)
        return;
    m_contrast = on;
    emit changed();
}

bool AccessibilityModel::highContrast() const
{
    return m_contrast;
}

void AccessibilityModel::setLargeText(bool on)
{
    if (m_large == on)
        return;
    m_large = on;
    emit changed();
}

bool AccessibilityModel::largeText() const
{
    return m_large;
}

void AccessibilityModel::setTextScale(int percent)
{
    // Clamp first and compare the stored result, so a request outside the range
    // that lands on the current value stays silent like any other repeat.
    const int clamped = qBound(kMinTextScale, percent, kMaxTextScale);
    if (m_scale == clamped)
        return;
    m_scale = clamped;
    emit changed();
}

int AccessibilityModel::textScale() const
{
    return m_scale;
}

void AccessibilityModel::setScreenReaderHints(bool on)
{
    if (m_hints == on)
        return;
    m_hints = on;
    emit changed();
}

bool AccessibilityModel::screenReaderHints() const
{
    return m_hints;
}

void AccessibilityModel::reset()
{
    // A model that never left its defaults has nothing to restore, so the
    // screen is not repainted for a reset that changes nothing.
    if (isDefault())
        return;

    m_motion = false;
    m_contrast = false;
    m_large = false;
    m_hints = false;
    m_scale = kDefaultTextScale;
    emit changed();
}

bool AccessibilityModel::isDefault() const
{
    return !m_motion && !m_contrast && !m_large && !m_hints
        && m_scale == kDefaultTextScale;
}

} // namespace beygram::telegram
