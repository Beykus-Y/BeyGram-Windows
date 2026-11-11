#include "ReactionUpdates.h"

#include "TelegramTypes.h"

namespace beygram::telegram {

// TDLib's updateMessageReactions carries the bare reaction list under
// `reactions`, which is exactly the shape reactionsFromMessage accepts, so the
// update parser reuses it instead of duplicating the entry rules. No logging,
// no side effects.
ReactionUpdate reactionUpdateFromUpdate(const QJsonObject& update)
{
    ReactionUpdate result;
    result.chatId = jsonInt64(update.value(QStringLiteral("chat_id")));
    result.messageId = jsonInt64(update.value(QStringLiteral("message_id")));
    result.reactions = reactionsFromMessage(update);
    return result;
}

bool isReactionUpdate(const QJsonObject& update)
{
    return update.value(QStringLiteral("@type")).toString()
        == QLatin1String("updateMessageReactions");
}

} // namespace beygram::telegram
