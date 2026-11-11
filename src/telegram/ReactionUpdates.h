#pragma once

#include "MessageReactions.h"

#include <QJsonObject>
#include <QtGlobal>

namespace beygram::telegram {

/// One updateMessageReactions payload reduced to the data the message model
/// needs. TDLib sends the bare reaction list under `reactions`, so the same
/// parser used for messages applies; missing fields fall back to zero/empty.
struct ReactionUpdate {
    qint64 chatId = 0;
    qint64 messageId = 0;
    MessageReactions reactions;
};

/// Parses an updateMessageReactions object. Tolerates a missing or malformed
/// `reactions` list and missing ids; the update type is not checked here, so
/// callers should gate on isReactionUpdate() first. Accepts ids as JSON
/// numbers or numeric strings.
[[nodiscard]] ReactionUpdate reactionUpdateFromUpdate(const QJsonObject& update);

/// True when the update is a TDLib updateMessageReactions.
[[nodiscard]] bool isReactionUpdate(const QJsonObject& update);

} // namespace beygram::telegram
