#include "ChatFolderFilters.h"

namespace beygram::telegram {

bool chatMatchesFolder(const ChatFolderFilter& filter, qint64 chatId, bool isContact,
                       bool isGroup, bool isChannel, bool isBot, bool muted, bool read,
                       bool archived)
{
    // An explicit exclusion always wins, over both the include list and every
    // category flag.
    if (filter.excludeChatIds.contains(chatId))
        return false;

    // The property exclusions are folder-wide, so they also apply to chats that
    // the include list names directly.
    if ((filter.excludeMuted && muted) || (filter.excludeRead && read)
        || (filter.excludeArchived && archived)) {
        return false;
    }

    // The include list overrides category matching only.
    if (filter.includeChatIds.contains(chatId))
        return true;

    // A non-contact is a private chat that is not a contact and is none of the
    // other kinds; groups, channels and bots have their own categories.
    const bool isNonContactPrivate = !isContact && !isGroup && !isChannel && !isBot;

    return (filter.contacts && isContact) || (filter.nonContacts && isNonContactPrivate)
        || (filter.groups && isGroup) || (filter.channels && isChannel)
        || (filter.bots && isBot);
}

} // namespace beygram::telegram
