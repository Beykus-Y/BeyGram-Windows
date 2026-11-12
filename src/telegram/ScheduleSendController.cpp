#include "ScheduleSendController.h"

#include <algorithm>
#include <limits>

namespace beygram::telegram {
namespace {

// TDLib declares send_date as int32; a date it cannot represent is rejected by
// the server rather than wrapped around locally.
constexpr qint64 kMaxSendDate = std::numeric_limits<qint32>::max();

} // namespace

ScheduleSendController::ScheduleSendController(QObject* parent)
    : QObject(parent)
{
}

void ScheduleSendController::setChat(qint64 chatId)
{
    if (m_chatId == chatId)
        return;

    m_chatId = chatId;
    emit changed();
}

qint64 ScheduleSendController::chatId() const
{
    return m_chatId;
}

void ScheduleSendController::setDateTime(const QDateTime& when)
{
    // Only the moment counts, not the time spec it was expressed in, so the
    // two values are compared as instants. Two invalid QDateTimes are not
    // reliably equal, hence the explicit "both unset" case.
    if (m_when.isValid() == when.isValid()
        && (!when.isValid()
            || m_when.toMSecsSinceEpoch() == when.toMSecsSinceEpoch()))
        return;

    m_when = when;
    emit changed();
}

QDateTime ScheduleSendController::dateTime() const
{
    return m_when;
}

bool ScheduleSendController::valid() const
{
    if (m_chatId == 0 || !m_when.isValid())
        return false;

    // Comparing against UTC makes the check independent of the time spec the
    // caller used for the selected moment.
    return m_when > QDateTime::currentDateTimeUtc();
}

qint32 ScheduleSendController::sendDate() const
{
    if (!valid())
        return 0;

    const qint64 seconds = m_when.toSecsSinceEpoch();
    return static_cast<qint32>(std::clamp<qint64>(seconds, 0, kMaxSendDate));
}

void ScheduleSendController::reset()
{
    if (m_chatId == 0 && !m_when.isValid())
        return;

    m_chatId = 0;
    m_when = QDateTime();
    emit changed();
}

bool ScheduleSendController::onlineOnly() const
{
    return m_onlineOnly;
}

void ScheduleSendController::setOnlineOnly(bool on)
{
    if (m_onlineOnly == on)
        return;

    m_onlineOnly = on;
    emit changed();
}

} // namespace beygram::telegram
