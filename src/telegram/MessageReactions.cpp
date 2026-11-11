#include "MessageReactions.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

namespace beygram::telegram {
namespace {

/// Maps TDLib's reactionType* class name to the short tag stored in
/// ReactionCount::type.
QString reactionKind(const QString& tdlibType)
{
    if (tdlibType == QLatin1String("reactionTypeEmoji"))
        return QStringLiteral("emoji");
    if (tdlibType == QLatin1String("reactionTypeCustomEmoji"))
        return QStringLiteral("custom_emoji");
    if (tdlibType == QLatin1String("reactionTypePaid"))
        return QStringLiteral("paid");
    return {};
}

void appendReaction(const QJsonObject& entry, QVector<ReactionCount>& out)
{
    const QJsonValue typeValue = entry.value(QStringLiteral("type"));
    if (!typeValue.isObject())
        return;

    const QJsonObject type = typeValue.toObject();
    const QString kind = reactionKind(type.value(QStringLiteral("@type")).toString());
    if (kind.isEmpty())
        return;

    // Pinned TDLib names the counter total_count; count is kept for the
    // messageReactions shape used by older revisions.
    const QJsonValue countValue = entry.contains(QStringLiteral("count"))
        ? entry.value(QStringLiteral("count"))
        : entry.value(QStringLiteral("total_count"));
    if (!countValue.isDouble())
        return;
    const int count = countValue.toInt(0);
    if (count < 0)
        return;

    ReactionCount reaction;
    reaction.type = kind;
    reaction.count = count;
    reaction.chosen = entry.value(QStringLiteral("is_chosen")).toBool(false);

    if (kind == QLatin1String("emoji"))
        reaction.emoji = type.value(QStringLiteral("emoji")).toString();
    else if (kind == QLatin1String("custom_emoji"))
        reaction.customEmojiId = jsonInt64(type.value(QStringLiteral("custom_emoji_id")));

    out.append(reaction);
}

QJsonObject emojiReactionType(const QString& emoji)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("reactionTypeEmoji")},
        {QStringLiteral("emoji"), emoji},
    };
}

QJsonObject customEmojiReactionType(qint64 customEmojiId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("reactionTypeCustomEmoji")},
        {QStringLiteral("custom_emoji_id"), customEmojiId},
    };
}

} // namespace

MessageReactions reactionsFromMessage(const QJsonObject& message)
{
    MessageReactions result;

    QJsonArray entries;
    const QJsonValue direct = message.value(QStringLiteral("reactions"));

    if (direct.isObject()) {
        const QJsonObject reactions = direct.toObject();
        entries = reactions.value(QStringLiteral("reactions")).toArray();
        result.canGetAddedReactions =
            reactions.value(QStringLiteral("can_get_added_reactions")).toBool(false);
    } else if (direct.isArray()) {
        // updateMessageReactions carries the bare list.
        entries = direct.toArray();
        result.canGetAddedReactions =
            message.value(QStringLiteral("can_get_added_reactions")).toBool(false);
    } else {
        // A TDLib message stores reactions under interaction_info.
        const QJsonObject interaction =
            message.value(QStringLiteral("interaction_info")).toObject();
        const QJsonObject reactions =
            interaction.value(QStringLiteral("reactions")).toObject();
        entries = reactions.value(QStringLiteral("reactions")).toArray();
        result.canGetAddedReactions =
            reactions.value(QStringLiteral("can_get_added_reactions")).toBool(false);
    }

    result.reactions.reserve(entries.size());
    for (const QJsonValue& value : entries) {
        if (value.isObject())
            appendReaction(value.toObject(), result.reactions);
    }

    return result;
}

QJsonObject sendReactionRequest(qint64 chatId, qint64 messageId, const QString& emoji, bool big)
{
    // TDLib 1.8.69 replaced sendMessageReaction with addMessageReaction; the
    // payload fields are the same.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("addMessageReaction")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("reaction_type"), emojiReactionType(emoji)},
        {QStringLiteral("is_big"), big},
        {QStringLiteral("update_recent_reactions"), true},
    };
}

QJsonObject sendReactionRequest(qint64 chatId, qint64 messageId, qint64 customEmojiId, bool big)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("addMessageReaction")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("reaction_type"), customEmojiReactionType(customEmojiId)},
        {QStringLiteral("is_big"), big},
        {QStringLiteral("update_recent_reactions"), true},
    };
}

QJsonObject clearReactionRequest(qint64 chatId, qint64 messageId)
{
    // removeMessageReaction requires a concrete ReactionType. This builder has
    // no type parameter, so the caller must fill reaction_type with the chosen
    // reaction before sending.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("removeMessageReaction")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("reaction_type"), QJsonValue::Null},
    };
}

} // namespace beygram::telegram
