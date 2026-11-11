#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One reaction entry shown on a message. Parsed from TDLib without touching
/// the UI, so the model layer can hold it directly.
struct ReactionCount {
    QString type;      // "emoji" | "custom_emoji" | "paid"
    QString emoji;     // for emoji
    qint64 customEmojiId = 0;
    int count = 0;
    bool chosen = false;
};

/// The reactions of one message. `canGetAddedReactions` mirrors TDLib's
/// can_get_added_reactions flag for the list.
struct MessageReactions {
    QVector<ReactionCount> reactions;
    bool canGetAddedReactions = false;
};

/// Parses the reactions of a TDLib message. Accepts message["reactions"] as a
/// messageReactions object, the nested interaction_info.reactions shape that
/// TDLib actually sends for a message, or a bare reaction list as carried by
/// updateMessageReactions.
[[nodiscard]] MessageReactions reactionsFromMessage(const QJsonObject& message);

/// Builds a TDLib request that adds an emoji reaction.
[[nodiscard]] QJsonObject sendReactionRequest(qint64 chatId, qint64 messageId,
                                              const QString& emoji,
                                              bool big = false);

/// Builds a TDLib request that adds a custom emoji reaction.
[[nodiscard]] QJsonObject sendReactionRequest(qint64 chatId, qint64 messageId,
                                              qint64 customEmojiId,
                                              bool big = false);

/// Builds a removeMessageReaction request with a null reaction_type. Pinned
/// TDLib requires the concrete chosen reaction type, so callers must fill
/// reaction_type before sending; the signature has no type parameter yet.
[[nodiscard]] QJsonObject clearReactionRequest(qint64 chatId, qint64 messageId);

} // namespace beygram::telegram
