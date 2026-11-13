#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One forward job: a single source chat, the messages to forward and every chat
/// that should receive them.
///
/// The vectors may contain non-positive ids and duplicates; validate() and the
/// builders normalise them (positive ids only, first occurrence wins). fromChatId
/// only has to be non-zero, because channel and group ids are negative.
struct ForwardBatchPlan {
    qint64 fromChatId = 0;
    QVector<qint64> messageIds;
    QVector<qint64> targetChatIds;
};

/// Plans a multi-target forward and builds the matching TDLib requests.
///
/// Limits: 1..100 unique message ids and 1..20 unique target chats. validate()
/// returns an empty string for a usable plan, otherwise one of "no-source",
/// "no-messages", "no-targets", "too-many-messages" or "too-many-targets".
/// The limits apply after normalisation, so duplicates do not consume the
/// budget.
///
/// buildAll() returns one forwardMessages request per target chat, in plan
/// order, and an empty vector for an invalid plan. build() is the single-request
/// convenience for the first target; it returns an empty object when the plan is
/// invalid. Both perform no I/O, no logging and add no @extra.
///
/// dropAuthor maps to send_copy: the message is re-sent without the original
/// sender. dropCaptions maps to remove_caption; TDLib only honours it together
/// with send_copy, so callers that want captions gone should drop the author
/// too.
class ForwardBatchPlanner final {
public:
    [[nodiscard]] static QJsonObject build(const ForwardBatchPlan& plan, bool dropAuthor = false,
                                           bool dropCaptions = false);
    [[nodiscard]] static QVector<QJsonObject> buildAll(const ForwardBatchPlan& plan,
                                                       bool dropAuthor = false,
                                                       bool dropCaptions = false);
    [[nodiscard]] static QString validate(const ForwardBatchPlan& plan);
};

} // namespace beygram::telegram
