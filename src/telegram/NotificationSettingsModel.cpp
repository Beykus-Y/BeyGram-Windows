#include "NotificationSettingsModel.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int MinutesPerDay = 24 * 60;

// A stored bound has to name a real minute on a clock, so anything outside the
// day is pulled back to the nearest edge instead of wrapping into a different
// hour than the caller asked for.
int clampedMinute(int minutes)
{
    return std::clamp(minutes, 0, MinutesPerDay - 1);
}

// A value that does not name a minute of the day cannot be compared to the
// window; the caller is expected to pass a real clock value.
bool isMinuteOfDay(int minutes)
{
    return minutes >= 0 && minutes < MinutesPerDay;
}

} // namespace

NotificationSettingsModel::NotificationSettingsModel(QObject* parent)
    : QObject(parent)
{
}

void NotificationSettingsModel::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit changed();
}

bool NotificationSettingsModel::enabled() const
{
    return m_enabled;
}

void NotificationSettingsModel::setSoundEnabled(bool enabled)
{
    if (m_sound == enabled)
        return;
    m_sound = enabled;
    emit changed();
}

bool NotificationSettingsModel::soundEnabled() const
{
    return m_sound;
}

void NotificationSettingsModel::setPreviewEnabled(bool enabled)
{
    if (m_preview == enabled)
        return;
    m_preview = enabled;
    emit changed();
}

bool NotificationSettingsModel::previewEnabled() const
{
    return m_preview;
}

void NotificationSettingsModel::setQuietHours(int startMinutes, int endMinutes)
{
    const int start = clampedMinute(startMinutes);
    const int end = clampedMinute(endMinutes);

    if (m_start == start && m_end == end)
        return;

    m_start = start;
    m_end = end;
    emit changed();
}

int NotificationSettingsModel::quietStartMinutes() const
{
    return m_start;
}

int NotificationSettingsModel::quietEndMinutes() const
{
    return m_end;
}

bool NotificationSettingsModel::quietHoursActive() const
{
    // Equal bounds are the settings screen's "off": a window that starts and
    // ends at the same minute would otherwise cover nothing or everything,
    // depending on which edge is included.
    return m_start != m_end;
}

bool NotificationSettingsModel::inQuietHours(int minutesOfDay) const
{
    if (!quietHoursActive() || !isMinuteOfDay(minutesOfDay))
        return false;

    if (m_start < m_end)
        return minutesOfDay >= m_start && minutesOfDay < m_end;

    // The window wraps around midnight: the evening part runs from the start to
    // the end of the day, the morning part from midnight to the end.
    return minutesOfDay >= m_start || minutesOfDay < m_end;
}

void NotificationSettingsModel::setDnd(bool dnd)
{
    if (m_dnd == dnd)
        return;
    m_dnd = dnd;
    emit changed();
}

bool NotificationSettingsModel::dnd() const
{
    return m_dnd;
}

bool NotificationSettingsModel::shouldNotify(int minutesOfDay, bool chatMuted) const
{
    if (!m_enabled || m_dnd || chatMuted)
        return false;
    return !inQuietHours(minutesOfDay);
}

} // namespace beygram::telegram
