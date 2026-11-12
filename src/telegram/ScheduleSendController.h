#pragma once

#include <QDateTime>
#include <QObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Holds the target chat and the delivery time of a message that is scheduled
/// instead of sent immediately, together with the "send when online"
/// alternative.
///
/// A chat id of zero means "no target" and an invalid QDateTime means "no
/// time", so both start out unset. valid() is the single gate for scheduling:
/// it needs a non-zero chat id and a time strictly after the current UTC time,
/// which is read on every call, so a selection that drifts into the past stops
/// being valid on its own. Group and channel ids are negative in TDLib, so any
/// non-zero chat id is accepted.
///
/// sendDate() exposes the time as Unix seconds for TDLib's int32 send_date
/// field: 0 when valid() is false, and saturated at INT32_MAX for dates beyond
/// 2038 because TDLib rejects values it cannot represent.
///
/// reset() clears the chat and the time but keeps the online-only preference,
/// which is an independent switch. changed() is emitted only when the stored
/// state actually changed.
class ScheduleSendController final : public QObject {
    Q_OBJECT

public:
    explicit ScheduleSendController(QObject* parent = nullptr);

    /// Sets the chat the scheduled message targets. Zero clears the target;
    /// every other value, including negative group and channel ids, is stored.
    void setChat(qint64 chatId);

    [[nodiscard]] qint64 chatId() const;

    /// Sets the delivery time. An invalid QDateTime clears it. The value is
    /// kept as given, so the caller's time zone survives.
    void setDateTime(const QDateTime& when);

    [[nodiscard]] QDateTime dateTime() const;

    /// True when a chat is set, the time is valid and it is still in the
    /// future compared to the current UTC time.
    [[nodiscard]] bool valid() const;

    /// The delivery time in Unix seconds, or 0 when valid() is false.
    [[nodiscard]] qint32 sendDate() const;

    /// Clears the chat and the time; onlineOnly() is untouched.
    void reset();

    [[nodiscard]] bool onlineOnly() const;
    void setOnlineOnly(bool on);

signals:
    /// Emitted whenever the chat, the time or the online-only flag changed.
    void changed();

private:
    qint64 m_chatId = 0;
    QDateTime m_when;
    bool m_onlineOnly = false;
};

} // namespace beygram::telegram
