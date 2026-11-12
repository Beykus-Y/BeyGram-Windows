#include "ReactionCatalog.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <limits>

namespace beygram::telegram {
namespace {

constexpr int kMinRecentReactionsLimit = 1;
constexpr int kMaxRecentReactionsLimit = 100;

// Maps a TDLib reactionType* class name, the Telegram API reactionEmoji/
// reactionCustomEmoji spelling, or an already short tag to the tag stored in
// AvailableReaction::type. Unknown names yield an empty string.
QString reactionKind(const QString& name)
{
    if (name == QLatin1String("reactionTypeEmoji")
        || name == QLatin1String("reactionEmoji")
        || name == QLatin1String("emoji")) {
        return QStringLiteral("emoji");
    }
    if (name == QLatin1String("reactionTypeCustomEmoji")
        || name == QLatin1String("reactionCustomEmoji")
        || name == QLatin1String("custom_emoji")) {
        return QStringLiteral("custom_emoji");
    }
    if (name == QLatin1String("reactionTypePaid")
        || name == QLatin1String("paid")) {
        return QStringLiteral("paid");
    }
    return {};
}

bool isReactionTypeName(const QString& name)
{
    return name.startsWith(QLatin1String("reactionType"))
        || name == QLatin1String("reactionEmoji")
        || name == QLatin1String("reactionCustomEmoji");
}

// Reads one ReactionType object. Emoji and custom emoji entries need their
// payload, so an empty emoji or a non-positive custom emoji id is rejected.
bool applyReactionType(const QJsonObject& type, AvailableReaction& reaction)
{
    const QString kind =
        reactionKind(type.value(QStringLiteral("@type")).toString());
    if (kind.isEmpty())
        return false;

    reaction.type = kind;
    if (kind == QLatin1String("emoji")) {
        reaction.emoji = type.value(QStringLiteral("emoji")).toString();
        if (reaction.emoji.isEmpty()) {
            // Telegram API reactionEmoji uses emoticon instead of emoji.
            reaction.emoji = type.value(QStringLiteral("emoticon")).toString();
        }
        return !reaction.emoji.isEmpty();
    }
    if (kind == QLatin1String("custom_emoji")) {
        reaction.customEmojiId =
            jsonInt64(type.value(QStringLiteral("custom_emoji_id")));
        if (reaction.customEmojiId <= 0) {
            // Telegram API reactionCustomEmoji uses document_id.
            reaction.customEmojiId =
                jsonInt64(type.value(QStringLiteral("document_id")));
        }
        return reaction.customEmojiId > 0;
    }
    return true;
}

// Reads a reaction value that is either a ReactionType object or a legacy
// emoji string (availableReaction.reaction and old reactions lists).
bool applyReactionValue(const QJsonValue& value, AvailableReaction& reaction)
{
    if (value.isString()) {
        const QString emoji = value.toString();
        if (emoji.isEmpty())
            return false;
        reaction.type = QStringLiteral("emoji");
        reaction.emoji = emoji;
        return true;
    }
    if (value.isObject())
        return applyReactionType(value.toObject(), reaction);
    return false;
}

// A string at the top of a reactions list is the legacy emoji spelling.
bool reactionFromEntry(const QJsonValue& entry, AvailableReaction& reaction)
{
    if (entry.isString()) {
        const QString emoji = entry.toString();
        if (emoji.isEmpty())
            return false;
        reaction.type = QStringLiteral("emoji");
        reaction.emoji = emoji;
        return true;
    }
    if (!entry.isObject())
        return false;

    const QJsonObject object = entry.toObject();
    const QString entryType = object.value(QStringLiteral("@type")).toString();

    bool known = false;
    if (isReactionTypeName(entryType)) {
        // A bare ReactionType, as carried by getRecentReactions and by the
        // chatAvailableReactionsSome.reactions list.
        known = applyReactionType(object, reaction);
    } else {
        // availableReaction.type is a ReactionType object in the pinned
        // schema; a short type string is accepted from trimmed payloads.
        const QJsonValue typeValue = object.value(QStringLiteral("type"));
        if (typeValue.isObject()) {
            known = applyReactionType(typeValue.toObject(), reaction);
        } else if (typeValue.isString()) {
            const QString kind = reactionKind(typeValue.toString());
            if (!kind.isEmpty()) {
                reaction.type = kind;
                if (kind == QLatin1String("emoji")) {
                    reaction.emoji =
                        object.value(QStringLiteral("emoji")).toString();
                    if (reaction.emoji.isEmpty()) {
                        reaction.emoji =
                            object.value(QStringLiteral("emoticon")).toString();
                    }
                    known = !reaction.emoji.isEmpty();
                } else if (kind == QLatin1String("custom_emoji")) {
                    reaction.customEmojiId =
                        jsonInt64(
                            object.value(QStringLiteral("custom_emoji_id")));
                    if (reaction.customEmojiId <= 0) {
                        reaction.customEmojiId =
                            jsonInt64(
                                object.value(QStringLiteral("document_id")));
                    }
                    known = reaction.customEmojiId > 0;
                } else {
                    known = true;
                }
            }
        }

        // Older payloads name the field reaction instead of type.
        if (!known) {
            known = applyReactionValue(object.value(QStringLiteral("reaction")),
                                       reaction);
        }

        // A flat object may carry only emoji or custom_emoji_id.
        if (!known) {
            const QString emoji =
                object.value(QStringLiteral("emoji")).toString();
            const qint64 customEmojiId =
                jsonInt64(object.value(QStringLiteral("custom_emoji_id")));
            if (!emoji.isEmpty()) {
                reaction.type = QStringLiteral("emoji");
                reaction.emoji = emoji;
                known = true;
            } else if (customEmojiId > 0) {
                reaction.type = QStringLiteral("custom_emoji");
                reaction.customEmojiId = customEmojiId;
                known = true;
            }
        }
    }

    if (!known)
        return false;

    reaction.title = object.value(QStringLiteral("title")).toString();

    // is_active is the TDLib spelling; the Telegram API shape uses the
    // inverted inactive flag. A non-boolean value keeps the default.
    if (object.value(QStringLiteral("is_active")).isBool()) {
        reaction.isActive =
            object.value(QStringLiteral("is_active")).toBool();
    } else if (object.value(QStringLiteral("inactive")).isBool()) {
        reaction.isActive =
            !object.value(QStringLiteral("inactive")).toBool();
    }

    // is_premium is the old TDLib spelling, needs_premium the pinned one, and
    // premium the Telegram API flag. The first boolean present wins.
    if (object.value(QStringLiteral("is_premium")).isBool()) {
        reaction.isPremium =
            object.value(QStringLiteral("is_premium")).toBool();
    } else if (object.value(QStringLiteral("needs_premium")).isBool()) {
        reaction.isPremium =
            object.value(QStringLiteral("needs_premium")).toBool();
    } else if (object.value(QStringLiteral("premium")).isBool()) {
        reaction.isPremium = object.value(QStringLiteral("premium")).toBool();
    }

    return true;
}

QVector<AvailableReaction> reactionsFromArray(const QJsonArray& entries)
{
    QVector<AvailableReaction> reactions;
    reactions.reserve(entries.size());
    for (const QJsonValue& entry : entries) {
        AvailableReaction reaction;
        if (reactionFromEntry(entry, reaction))
            reactions.append(reaction);
    }
    return reactions;
}

// max_reaction_count is int32 in the schema; a negative or missing value is
// reported as 0, and a larger number is clamped to the int range.
int maxReactionCountFrom(const QJsonObject& object)
{
    const qint64 count =
        jsonInt64(object.value(QStringLiteral("max_reaction_count")));
    if (count <= 0)
        return 0;
    return static_cast<int>(
        std::min<qint64>(count, std::numeric_limits<int>::max()));
}

bool looksLikeReactionTypeList(const QJsonArray& entries)
{
    if (entries.isEmpty())
        return false;
    for (const QJsonValue& entry : entries) {
        if (!entry.isObject())
            return false;
        const QString type =
            entry.toObject().value(QStringLiteral("@type")).toString();
        if (!isReactionTypeName(type))
            return false;
    }
    return true;
}

} // namespace

ReactionCatalogState reactionCatalogFromResponse(const QJsonObject& response)
{
    ReactionCatalogState state;

    // A chat-like payload nests its chatAvailableReactions object under
    // available_reactions. Do not unwrap when the response already carries a
    // catalog list, so a direct answer always wins over a nested one.
    QJsonObject catalog = response;
    const QJsonValue nestedValue =
        response.value(QStringLiteral("available_reactions"));
    if (!response.contains(QStringLiteral("top_reactions"))
        && !response.contains(QStringLiteral("recent_reactions"))
        && !response.contains(QStringLiteral("reactions"))
        && nestedValue.isObject()) {
        catalog = nestedValue.toObject();
    }

    state.maxReactionCount = maxReactionCountFrom(catalog);
    if (state.maxReactionCount == 0)
        state.maxReactionCount = maxReactionCountFrom(response);

    const QJsonValue topValue = catalog.value(QStringLiteral("top_reactions"));
    if (topValue.isArray())
        state.topReactions = reactionsFromArray(topValue.toArray());

    const QJsonValue recentValue =
        catalog.value(QStringLiteral("recent_reactions"));
    if (recentValue.isArray())
        state.recentReactions = reactionsFromArray(recentValue.toArray());

    // A lone reactions list is either a getRecentReactions answer (bare
    // ReactionType entries), the old availableReactions catalog, or a chat's
    // allowed reaction list. The payload type, when present, decides; a list
    // of bare reaction types otherwise means recent reactions.
    const QJsonValue reactionsValue =
        catalog.value(QStringLiteral("reactions"));
    if (!topValue.isArray() && !recentValue.isArray()
        && reactionsValue.isArray()) {
        const QJsonArray entries = reactionsValue.toArray();
        const QString catalogType =
            catalog.value(QStringLiteral("@type")).toString();
        const bool catalogAnswer =
            catalogType == QLatin1String("availableReactions")
            || catalogType == QLatin1String("messages.availableReactions");
        const bool recentAnswer =
            catalogType == QLatin1String("reactions")
            || catalogType == QLatin1String("messages.reactions");
        const bool chatList =
            catalogType == QLatin1String("chatAvailableReactionsSome");
        if (catalogAnswer || chatList)
            state.topReactions = reactionsFromArray(entries);
        else if (recentAnswer || looksLikeReactionTypeList(entries))
            state.recentReactions = reactionsFromArray(entries);
        else
            state.topReactions = reactionsFromArray(entries);
    }

    return state;
}

QJsonObject getAvailableReactionsRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getAvailableReactions")},
    };
}

QJsonObject getRecentReactionsRequest(int limit)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getRecentReactions")},
        {QStringLiteral("limit"),
         std::clamp(limit, kMinRecentReactionsLimit, kMaxRecentReactionsLimit)},
    };
}

QJsonObject clearRecentReactionsRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("clearRecentReactions")},
    };
}

} // namespace beygram::telegram
