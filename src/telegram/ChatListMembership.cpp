#include "ChatListMembership.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

namespace beygram::telegram {

// TDLib names the main and archive lists with these @type values and puts the
// folder identifier inside the chatListFolder object.
ChatListMembershipState applyPosition(ChatListMembershipState state, const QJsonObject& position)
{
    const QJsonObject list = position.value(QStringLiteral("list")).toObject();
    const QString listType = list.value(QStringLiteral("@type")).toString();
    const qint64 order = jsonInt64(position.value(QStringLiteral("order")));
    const bool pinned = position.value(QStringLiteral("is_pinned")).toBool();
    // TDLib reports order 0 for a list the chat has left, so the same update
    // both adds and removes membership.
    const bool present = order != 0;

    if (listType == QLatin1String("chatListMain")) {
        state.inMain = present;
        return state;
    }
    if (listType == QLatin1String("chatListArchive")) {
        state.inArchive = present;
        return state;
    }
    if (listType == QLatin1String("chatListFolder")) {
        // Without a folder identifier the list cannot be keyed, so there is
        // nothing to record or remove.
        const qint64 folderId = jsonInt64(list.value(QStringLiteral("chat_folder_id")));
        if (folderId <= 0)
            return state;
        if (present)
            state.folderPositions.insert(folderId, ChatListPosition{order, pinned});
        else
            state.folderPositions.remove(folderId);
        return state;
    }

    // A missing or unknown list identifies no membership; keep the state as is
    // rather than guessing a list.
    return state;
}

ChatListMembershipState membershipFromPositionUpdate(const QJsonObject& update)
{
    ChatListMembershipState state;

    const QJsonValue positionsValue = update.value(QStringLiteral("positions"));
    if (positionsValue.isArray()) {
        // The positions array is the chat's authoritative membership, so it
        // replaces the state: lists absent from the array end up not joined.
        const QJsonArray positions = positionsValue.toArray();
        for (const QJsonValue& value : positions) {
            if (!value.isObject())
                continue;
            state = applyPosition(state, value.toObject());
        }
        return state;
    }

    return applyPosition(state, update.value(QStringLiteral("position")).toObject());
}

} // namespace beygram::telegram
