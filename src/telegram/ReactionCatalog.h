#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders and parsers for TDLib reaction catalogs. They contain no
/// logging, no secrets and no @extra; the transport adds correlation.

/// One reaction offered by a reaction catalog.
///
/// type uses the same short tags as MessageReactions: "emoji",
/// "custom_emoji" or "paid". emoji is filled only for "emoji" and
/// customEmojiId only for "custom_emoji".
///
/// The pinned TDLib schema (1.8.69, commit 4c6377aa) defines
///   availableReaction type:ReactionType needs_premium:Bool = AvailableReaction;
/// so a pinned payload supplies type and needs_premium (mapped to isPremium),
/// leaves title empty and keeps isActive true. Older payloads and the Telegram
/// API shape carry reaction:string plus title, is_active/is_premium (or the
/// inverted inactive/premium flags); those fields are read when present, and
/// isActive/isPremium fall back to their defaults otherwise.
struct AvailableReaction {
    QString type;              // "emoji" | "custom_emoji" | "paid"
    QString emoji;             // for "emoji"
    qint64 customEmojiId = 0;  // for "custom_emoji"
    QString title;             // optional display title; may be empty
    bool isActive = true;
    bool isPremium = false;
};

/// The catalog a reaction picker needs. topReactions is the server's top list
/// and recentReactions its recently used list; both keep server order and may
/// overlap. maxReactionCount is 0 when the payload carries no usable
/// max_reaction_count (the pinned availableReactions shape does not).
struct ReactionCatalogState {
    QVector<AvailableReaction> topReactions;
    QVector<AvailableReaction> recentReactions;
    int maxReactionCount = 0;
};

/// Parses a reaction catalog payload.
///
/// Accepted shapes:
///   - availableReactions, the getAvailableReactions answer, with
///     top_reactions and recent_reactions;
///   - reactions, the getRecentReactions answer, whose `reactions` list is
///     returned as recentReactions;
///   - chatAvailableReactionsSome/All, or a chat-like object that nests it
///     under available_reactions; its reactions list is returned as
///     topReactions and its max_reaction_count is read.
///
/// popular_reactions has no field in ReactionCatalogState and is ignored.
/// Entries without a usable reaction type are skipped, while malformed
/// neighbours do not discard the rest of a list. Order and duplicates are
/// preserved. Missing fields keep the struct defaults (isActive true,
/// isPremium false, title empty).
[[nodiscard]] ReactionCatalogState reactionCatalogFromResponse(
    const QJsonObject& response);

/// Builds getAvailableReactions. The pinned TDLib answers a message's catalog
/// through getMessageAvailableReactions instead; this builder exists for the
/// catalog API used by this module and takes no arguments.
[[nodiscard]] QJsonObject getAvailableReactionsRequest();

/// Builds getRecentReactions with limit clamped to 1..100, the range used by
/// the other TDLib list requests in this client. The pinned TDLib keeps only
/// clearRecentReactions from the recent-reaction methods; this builder exists
/// for the catalog API used by this module.
[[nodiscard]] QJsonObject getRecentReactionsRequest(int limit = 50);

/// Builds clearRecentReactions, which has no arguments.
[[nodiscard]] QJsonObject clearRecentReactionsRequest();

} // namespace beygram::telegram
