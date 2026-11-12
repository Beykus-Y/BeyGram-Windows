#pragma once

#include <QVector>

namespace beygram::telegram {

/// One TDLib chat folder's inclusion rules, reduced to the fields that decide
/// whether a single chat belongs to the folder.
///
/// The struct holds no pointers, no session state and no secrets, so it can be
/// cached per account and copied across threads. Every flag starts false: an
/// empty filter selects no category at all and therefore matches no chat. There
/// are no implicit category defaults; a caller that wants the main list builds
/// the filter it needs explicitly.
struct ChatFolderFilter {
    bool contacts = false;
    bool nonContacts = false;
    bool groups = false;
    bool channels = false;
    bool bots = false;
    bool excludeMuted = false;
    bool excludeRead = false;
    bool excludeArchived = false;
    QVector<qint64> includeChatIds;
    QVector<qint64> excludeChatIds;
};

/// Decides whether one chat belongs to the folder described by filter.
///
/// Precedence, highest first:
///
///   1. excludeChatIds: a listed chat is never part of the folder, even when it
///      is also in includeChatIds or matches a category.
///   2. excludeMuted / excludeRead / excludeArchived: a chat carrying an
///      excluded property is never part of the folder, including chats named by
///      includeChatIds. These are folder-wide rules, not category rules.
///   3. includeChatIds: a listed chat is part of the folder without consulting
///      the category flags.
///   4. Category flags: contacts, nonContacts, groups, channels and bots are
///      ORed, so a chat matching any selected category is included.
///
/// A chat is a non-contact private chat only when it is none of isContact,
/// isGroup, isChannel and isBot, so groups, channels and bots never fall into
/// the nonContacts category. Category flags are independent: a chat that
/// reports several kinds at once is included when any of its kinds is selected.
[[nodiscard]] bool chatMatchesFolder(const ChatFolderFilter& filter, qint64 chatId,
                                     bool isContact, bool isGroup, bool isChannel,
                                     bool isBot, bool muted, bool read, bool archived);

} // namespace beygram::telegram
