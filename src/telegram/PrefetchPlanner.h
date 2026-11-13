#pragma once

#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One media file the client may fetch before the user asks for it.
///
/// The struct is a plain value: chatId and messageId say where the file lives,
/// so a planned request can be traced back to its message, and fileId is the
/// TDLib file the downloader needs. priority is the caller's own ranking and is
/// returned unchanged; the planner only uses it for ordering.
struct PrefetchRequest {
    qint64 chatId = 0;
    qint64 messageId = 0;
    int fileId = 0;
    int priority = 0;

    bool operator==(const PrefetchRequest& other) const = default;
};

/// Turns a pool of prefetch candidates into the ordered work list the
/// downloader should run, at most `budget` entries long.
///
/// Only chats named in `visibleChatIds` or `recentChatIds` are planned: media
/// for a chat the user is neither looking at nor has just left is not worth the
/// bandwidth, so a candidate from any other chat is dropped. Both lists are
/// ordered by relevance, first entry first. A chat named in both lists keeps
/// its visible rank, and a repeated id inside one list is counted once.
///
/// The plan is ordered by:
///
///  1. visible chats before recent chats,
///  2. within a list, the order the caller passed,
///  3. within a chat, higher priority before lower,
///  4. within a chat and priority, newer (larger) messageId before older.
///
/// Candidates sharing a fileId describe one download, so only one of them
/// survives: the one with the highest priority. When priorities tie the copy in
/// the best ranked chat wins, then the newest message, so the survivor does not
/// depend on the order the candidates arrived in. fileId ascending breaks the
/// remaining ties, which makes the plan identical for any permutation of the
/// same candidates.
///
/// A fileId that is not positive names no file and is dropped. `budget` is
/// clamped to 1..64, so a plan is never empty when a request is available and
/// never longer than the downloader can usefully queue. Empty inputs produce an
/// empty plan.
class PrefetchPlanner final {
public:
    [[nodiscard]] static QVector<PrefetchRequest> plan(const QVector<qint64>& visibleChatIds,
                                                       const QVector<qint64>& recentChatIds,
                                                       const QVector<PrefetchRequest>& candidates,
                                                       int budget = 20);
};

} // namespace beygram::telegram
