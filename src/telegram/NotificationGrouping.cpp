#include "NotificationGrouping.h"

#include <QHash>

#include <algorithm>
#include <limits>

namespace beygram::telegram {

QVector<GroupedNotification> groupNotifications(
    const QVector<GroupedNotification>& incoming, int maxGroups, int maxPerGroup)
{
    if (maxGroups <= 0 || incoming.isEmpty())
        return {};

    QVector<GroupedNotification> groups;
    groups.reserve(incoming.size());

    QHash<qint64, qsizetype> groupIndexByChatId;
    groupIndexByChatId.reserve(incoming.size());

    for (const GroupedNotification& entry : incoming) {
        if (entry.chatId <= 0)
            continue;

        const auto existing = groupIndexByChatId.constFind(entry.chatId);
        if (existing != groupIndexByChatId.cend()) {
            GroupedNotification& group = groups[*existing];

            // Summing in qint64 keeps a pathological batch from wrapping the
            // int count; the result still saturates instead of going negative.
            const qint64 summed =
                static_cast<qint64>(group.count) + std::max(entry.count, 0);
            group.count = summed > std::numeric_limits<int>::max()
                              ? std::numeric_limits<int>::max()
                              : static_cast<int>(summed);

            group.mention = group.mention || entry.mention;
            if (entry.newestDate >= group.newestDate) {
                group.newestDate = entry.newestDate;
                group.title = entry.title;
                group.body = entry.body;
            }
            continue;
        }

        GroupedNotification group;
        group.chatId = entry.chatId;
        group.title = entry.title;
        group.body = entry.body;
        group.count = std::max(entry.count, 0);
        group.newestDate = entry.newestDate;
        group.mention = entry.mention;
        groups.append(group);
        groupIndexByChatId.insert(entry.chatId, groups.size() - 1);
    }

    std::sort(groups.begin(), groups.end(),
              [](const GroupedNotification& left, const GroupedNotification& right) {
                  if (left.newestDate != right.newestDate)
                      return left.newestDate > right.newestDate;
                  return left.chatId < right.chatId;
              });

    if (groups.size() > maxGroups)
        groups.resize(maxGroups);

    if (maxPerGroup > 0) {
        for (GroupedNotification& group : groups)
            group.count = std::min(group.count, maxPerGroup);
    }

    return groups;
}

} // namespace beygram::telegram
