#include "StealthSchedule.h"

#include <QChar>
#include <QLatin1Char>

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int MinutesPerDay = 24 * 60;

// The separator of the window text: a spaced en dash, written as a code point
// so the source stays ASCII and the literal does not depend on the compiler's
// source charset, matching the labels in the rest of this tree.
constexpr char16_t kEnDash = 0x2013;

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

// Minutes of the day as the settings screen prints them: "22:00", "08:05".
// Every stored bound is clamped to 0..1439, so the value always names a real
// minute of the day.
QString clockText(int minutesOfDay)
{
    const int hour = minutesOfDay / 60;
    const int minute = minutesOfDay % 60;

    return QString::number(hour).rightJustified(2, QLatin1Char('0'))
        + QLatin1Char(':')
        + QString::number(minute).rightJustified(2, QLatin1Char('0'));
}

} // namespace

StealthSchedule::StealthSchedule(QObject* parent)
    : QObject(parent)
{
}

void StealthSchedule::setWindow(int startMinutes, int endMinutes)
{
    const int start = clampedMinute(startMinutes);
    const int end = clampedMinute(endMinutes);

    if (m_window.startMinutes == start && m_window.endMinutes == end)
        return;

    m_window.startMinutes = start;
    m_window.endMinutes = end;
    emit changed();
}

int StealthSchedule::startMinutes() const
{
    return m_window.startMinutes;
}

int StealthSchedule::endMinutes() const
{
    return m_window.endMinutes;
}

void StealthSchedule::setEnabled(bool enabled)
{
    if (m_window.enabled == enabled)
        return;

    m_window.enabled = enabled;
    emit changed();
}

bool StealthSchedule::enabled() const
{
    return m_window.enabled;
}

bool StealthSchedule::activeAt(int minutesOfDay) const
{
    if (!isConfigured() || !isMinuteOfDay(minutesOfDay))
        return false;

    if (m_window.startMinutes < m_window.endMinutes)
        return minutesOfDay >= m_window.startMinutes && minutesOfDay < m_window.endMinutes;

    // The window wraps around midnight: the evening part runs from the start to
    // the end of the day, the morning part from midnight to the end.
    return minutesOfDay >= m_window.startMinutes || minutesOfDay < m_window.endMinutes;
}

int StealthSchedule::minutesUntilStart(int minutesOfDay) const
{
    // A running window has no start to wait for, and an off or invalid one has
    // no start at all.
    if (!isConfigured() || !isMinuteOfDay(minutesOfDay) || activeAt(minutesOfDay))
        return -1;

    int delta = m_window.startMinutes - minutesOfDay;
    if (delta <= 0)
        delta += MinutesPerDay;
    return delta;
}

QString StealthSchedule::windowText() const
{
    // Equal bounds and the off switch are the settings screen's "no window";
    // there is nothing to print, so the screen shows the off state instead of a
    // fake "22:00 - 22:00".
    if (!isConfigured())
        return QStringLiteral("Off");

    return clockText(m_window.startMinutes) + QLatin1Char(' ')
        + QChar(kEnDash) + QLatin1Char(' ')
        + clockText(m_window.endMinutes);
}

void StealthSchedule::reset()
{
    // A schedule that never left its defaults has nothing to restore, so the
    // screen is not repainted for a reset that changes nothing.
    if (m_window.startMinutes == 0 && m_window.endMinutes == 0 && !m_window.enabled)
        return;

    m_window = StealthWindow{};
    emit changed();
}

bool StealthSchedule::isConfigured() const
{
    // Equal bounds are the settings screen's "off": a window that starts and
    // ends at the same minute would otherwise cover nothing or everything,
    // depending on which edge is included.
    return m_window.enabled && m_window.startMinutes != m_window.endMinutes;
}

} // namespace beygram::telegram
