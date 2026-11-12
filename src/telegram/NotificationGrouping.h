#pragma once

#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One desktop notification that is ready to be shown, before grouping. A
/// caller collects one entry per message that `notificationForMessage` approved
/// and hands the batch to `groupNotifications`.
struct GroupedNotification {
    qint64 chatId = 0;     // non-positive ids are ignored
    QString title;         // taken from the newest entry of the chat
    QString body;          // taken from the newest entry of the chat
    int count = 0;         // messages represented by this entry
    qint64 newestDate = 0; // Unix time of the newest message
    bool mention = false;  // true when any message mentioned the user
};

/// Merges `incoming` by chatId: counts add up, title/body come from the entry
/// with the greatest newestDate (a later entry wins a tie), mention is OR-ed
/// and newestDate is the maximum. Groups are sorted by newestDate, newest
/// first, with chatId ascending as the tiebreak so the order is deterministic.
/// At most `maxGroups` groups are returned; a non-positive `maxGroups` returns
/// none. Each count is clamped to `maxPerGroup`, unless that value is not
/// positive, which leaves counts uncapped.
[[nodiscard]] QVector<GroupedNotification> groupNotifications(
    const QVector<GroupedNotification>& incoming,
    int maxGroups = 10,
    int maxPerGroup = 5);

} // namespace beygram::telegram
