#include "telegram/ChatFolderFilters.h"

#include <QtTest>

using namespace beygram::telegram;

namespace {

constexpr qint64 kChatId = 7;

ChatFolderFilter categoryFilter(bool contacts = false, bool nonContacts = false,
                                bool groups = false, bool channels = false,
                                bool bots = false)
{
    ChatFolderFilter filter;
    filter.contacts = contacts;
    filter.nonContacts = nonContacts;
    filter.groups = groups;
    filter.channels = channels;
    filter.bots = bots;
    return filter;
}

struct MatrixRow {
    const char* label;
    ChatFolderFilter filter;
    bool isContact;
    bool isGroup;
    bool isChannel;
    bool isBot;
    bool muted;
    bool read;
    bool archived;
    bool expected;
};

} // namespace

class ChatFolderFiltersTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsSelectNothing();
    void contactsCategory();
    void nonContactsCategoryExcludesOtherKinds();
    void groupsCategory();
    void channelsCategory();
    void botsCategory();
    void categoryFlagsAreOred();
    void includeListOverridesCategory();
    void excludeListWinsOverIncludeAndCategory();
    void excludeMutedApplies();
    void excludeReadApplies();
    void excludeArchivedApplies();
    void exclusionsApplyToIncludedChats();
    void identifiersAreMatchedExactly();
    void filterMatrix();
};

void ChatFolderFiltersTest::defaultsSelectNothing()
{
    const ChatFolderFilter filter;
    QVERIFY(!filter.contacts);
    QVERIFY(!filter.nonContacts);
    QVERIFY(!filter.groups);
    QVERIFY(!filter.channels);
    QVERIFY(!filter.bots);
    QVERIFY(!filter.excludeMuted);
    QVERIFY(!filter.excludeRead);
    QVERIFY(!filter.excludeArchived);
    QVERIFY(filter.includeChatIds.isEmpty());
    QVERIFY(filter.excludeChatIds.isEmpty());

    // No category is selected and nothing is listed explicitly, so even a
    // plain private chat is not part of the folder.
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, false, false, false, false,
                               false));
}

void ChatFolderFiltersTest::contactsCategory()
{
    const ChatFolderFilter filter = categoryFilter(true);

    QVERIFY(chatMatchesFolder(filter, kChatId, true, false, false, false, false, false,
                              false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, false, false, false, false,
                               false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, true, false, false, false, false,
                               false));
}

void ChatFolderFiltersTest::nonContactsCategoryExcludesOtherKinds()
{
    const ChatFolderFilter filter = categoryFilter(false, true);

    // A private chat that is not a contact is the only non-contact.
    QVERIFY(chatMatchesFolder(filter, kChatId, false, false, false, false, false, false,
                              false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, true, false, false, false, false, false,
                               false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, true, false, false, false, false,
                               false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, true, false, false, false,
                               false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, false, true, false, false,
                               false));
}

void ChatFolderFiltersTest::groupsCategory()
{
    const ChatFolderFilter filter = categoryFilter(false, false, true);

    QVERIFY(chatMatchesFolder(filter, kChatId, false, true, false, false, false, false,
                              false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, true, false, false, false,
                               false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, false, false, false, false,
                               false));
}

void ChatFolderFiltersTest::channelsCategory()
{
    const ChatFolderFilter filter = categoryFilter(false, false, false, true);

    QVERIFY(chatMatchesFolder(filter, kChatId, false, false, true, false, false, false,
                              false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, true, false, false, false, false,
                               false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, false, false, false, false,
                               false));
}

void ChatFolderFiltersTest::botsCategory()
{
    const ChatFolderFilter filter = categoryFilter(false, false, false, false, true);

    QVERIFY(chatMatchesFolder(filter, kChatId, false, false, false, true, false, false,
                              false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, false, false, false, false,
                               false));

    // The bot kind wins regardless of the other reported kinds.
    QVERIFY(chatMatchesFolder(filter, kChatId, true, false, false, true, false, false,
                              false));
}

void ChatFolderFiltersTest::categoryFlagsAreOred()
{
    const ChatFolderFilter filter = categoryFilter(true, false, true);

    QVERIFY(chatMatchesFolder(filter, kChatId, true, false, false, false, false, false,
                              false));
    QVERIFY(chatMatchesFolder(filter, kChatId, false, true, false, false, false, false,
                              false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, false, false, false, false,
                               false));

    // A chat that reports several kinds is included when any kind is selected.
    const ChatFolderFilter channelOnly = categoryFilter(false, false, false, true);
    QVERIFY(chatMatchesFolder(channelOnly, kChatId, false, true, true, false, false, false,
                              false));
}

void ChatFolderFiltersTest::includeListOverridesCategory()
{
    ChatFolderFilter filter; // no category selected
    filter.includeChatIds = {kChatId, 11};

    QVERIFY(chatMatchesFolder(filter, kChatId, false, false, false, false, false, false,
                              false));
    QVERIFY(chatMatchesFolder(filter, 11, true, false, false, false, false, false, false));

    // A chat that is not listed still falls back to the category flags, which
    // are all off here.
    QVERIFY(!chatMatchesFolder(filter, 12, true, true, false, false, false, false, false));

    // The include list overrides a category that would otherwise reject the
    // chat: a group is included although only contacts is selected.
    ChatFolderFilter contactsWithInclude = categoryFilter(true);
    contactsWithInclude.includeChatIds = {kChatId};
    QVERIFY(chatMatchesFolder(contactsWithInclude, kChatId, false, true, false, false,
                              false, false, false));
}

void ChatFolderFiltersTest::excludeListWinsOverIncludeAndCategory()
{
    ChatFolderFilter filter = categoryFilter(true, false, true);
    filter.includeChatIds = {kChatId};
    filter.excludeChatIds = {kChatId};

    // The chat is named by both lists and matches a category, yet exclusion wins.
    QVERIFY(!chatMatchesFolder(filter, kChatId, true, false, false, false, false, false,
                               false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, true, false, false, false, false,
                               false));

    // Other chats keep the normal category behaviour.
    QVERIFY(chatMatchesFolder(filter, 8, true, false, false, false, false, false, false));
}

void ChatFolderFiltersTest::excludeMutedApplies()
{
    ChatFolderFilter filter = categoryFilter(true);
    filter.excludeMuted = true;

    QVERIFY(!chatMatchesFolder(filter, kChatId, true, false, false, false, true, false,
                               false));
    QVERIFY(chatMatchesFolder(filter, kChatId, true, false, false, false, false, false,
                              false));

    // The flag alone does not include anything: no category is selected.
    ChatFolderFilter exclusionOnly;
    exclusionOnly.excludeMuted = true;
    QVERIFY(!chatMatchesFolder(exclusionOnly, kChatId, true, false, false, false, false,
                               false, false));
}

void ChatFolderFiltersTest::excludeReadApplies()
{
    ChatFolderFilter filter = categoryFilter(false, true);
    filter.excludeRead = true;

    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, false, false, false, true,
                               false));
    QVERIFY(chatMatchesFolder(filter, kChatId, false, false, false, false, false, false,
                              false));

    ChatFolderFilter exclusionOnly;
    exclusionOnly.excludeRead = true;
    QVERIFY(!chatMatchesFolder(exclusionOnly, kChatId, false, false, false, false, false,
                               true, false));
}

void ChatFolderFiltersTest::excludeArchivedApplies()
{
    ChatFolderFilter filter = categoryFilter(false, false, true);
    filter.excludeArchived = true;

    QVERIFY(!chatMatchesFolder(filter, kChatId, false, true, false, false, false, false,
                               true));
    QVERIFY(chatMatchesFolder(filter, kChatId, false, true, false, false, false, false,
                              false));

    ChatFolderFilter exclusionOnly;
    exclusionOnly.excludeArchived = true;
    QVERIFY(!chatMatchesFolder(exclusionOnly, kChatId, false, true, false, false, false,
                               false, true));
}

void ChatFolderFiltersTest::exclusionsApplyToIncludedChats()
{
    ChatFolderFilter filter;
    filter.includeChatIds = {kChatId};
    filter.excludeMuted = true;
    filter.excludeRead = true;
    filter.excludeArchived = true;

    // Each property exclusion overrides the include list on its own.
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, false, false, true, false,
                               false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, false, false, false, true,
                               false));
    QVERIFY(!chatMatchesFolder(filter, kChatId, false, false, false, false, false, false,
                               true));
    QVERIFY(chatMatchesFolder(filter, kChatId, false, false, false, false, false, false,
                              false));

    // A chat outside the include list is unaffected by these flags alone.
    QVERIFY(!chatMatchesFolder(filter, 8, false, false, false, false, false, false,
                               false));
}

void ChatFolderFiltersTest::identifiersAreMatchedExactly()
{
    ChatFolderFilter filter = categoryFilter(true);
    filter.includeChatIds = {0, -1001234567890LL, 4242424242LL};
    filter.excludeChatIds = {-5};

    QVERIFY(chatMatchesFolder(filter, 0, false, false, false, false, false, false, false));
    QVERIFY(chatMatchesFolder(filter, -1001234567890LL, false, false, false, false, false,
                              false, false));
    QVERIFY(chatMatchesFolder(filter, 4242424242LL, false, false, false, false, false,
                              false, false));
    QVERIFY(!chatMatchesFolder(filter, 1001234567890LL, false, false, false, false, false,
                               false, false));
    QVERIFY(!chatMatchesFolder(filter, -5, true, false, false, false, false, false, false));

    // Excluding -5 must not affect a different id that matches a category.
    QVERIFY(chatMatchesFolder(filter, 5, true, false, false, false, false, false, false));
}

void ChatFolderFiltersTest::filterMatrix()
{
    ChatFolderFilter empty;
    ChatFolderFilter contacts = categoryFilter(true);
    ChatFolderFilter nonContacts = categoryFilter(false, true);
    ChatFolderFilter groups = categoryFilter(false, false, true);
    ChatFolderFilter channels = categoryFilter(false, false, false, true);
    ChatFolderFilter bots = categoryFilter(false, false, false, false, true);
    ChatFolderFilter contactsAndGroups = categoryFilter(true, false, true);

    ChatFolderFilter includeOnly;
    includeOnly.includeChatIds = {kChatId};

    ChatFolderFilter includeAndExclude;
    includeAndExclude.includeChatIds = {kChatId};
    includeAndExclude.excludeChatIds = {kChatId};

    ChatFolderFilter mutedContacts = categoryFilter(true);
    mutedContacts.excludeMuted = true;

    ChatFolderFilter readContacts = categoryFilter(true);
    readContacts.excludeRead = true;

    ChatFolderFilter archivedContacts = categoryFilter(true);
    archivedContacts.excludeArchived = true;

    ChatFolderFilter includedButMuted = includeOnly;
    includedButMuted.excludeMuted = true;

    // label, filter, contact, group, channel, bot, muted, read, archived, expected
    const QVector<MatrixRow> rows = {
        {"empty filter rejects a private chat", empty, false, false, false, false, false,
         false, false, false},
        {"empty filter rejects a contact", empty, true, false, false, false, false, false,
         false, false},
        {"empty filter rejects a group", empty, false, true, false, false, false, false,
         false, false},
        {"contacts accepts a contact", contacts, true, false, false, false, false, false,
         false, true},
        {"contacts rejects a private non-contact", contacts, false, false, false, false,
         false, false, false, false},
        {"contacts rejects a group", contacts, false, true, false, false, false, false,
         false, false},
        {"nonContacts accepts a private non-contact", nonContacts, false, false, false,
         false, false, false, false, true},
        {"nonContacts rejects a contact", nonContacts, true, false, false, false, false,
         false, false, false},
        {"nonContacts rejects a group", nonContacts, false, true, false, false, false,
         false, false, false},
        {"nonContacts rejects a channel", nonContacts, false, false, true, false, false,
         false, false, false},
        {"nonContacts rejects a bot", nonContacts, false, false, false, true, false, false,
         false, false},
        {"groups accepts a group", groups, false, true, false, false, false, false, false,
         true},
        {"groups rejects a channel", groups, false, false, true, false, false, false, false,
         false},
        {"channels accepts a channel", channels, false, false, true, false, false, false,
         false, true},
        {"channels rejects a group", channels, false, true, false, false, false, false,
         false, false},
        {"bots accepts a bot", bots, false, false, false, true, false, false, false, true},
        {"bots rejects a human private chat", bots, false, false, false, false, false,
         false, false, false},
        {"ORed categories accept a contact", contactsAndGroups, true, false, false, false,
         false, false, false, true},
        {"ORed categories accept a group", contactsAndGroups, false, true, false, false,
         false, false, false, true},
        {"ORed categories reject a private non-contact", contactsAndGroups, false, false,
         false, false, false, false, false, false},
        {"excludeMuted rejects a muted contact", mutedContacts, true, false, false, false,
         true, false, false, false},
        {"excludeMuted keeps an unmuted contact", mutedContacts, true, false, false, false,
         false, false, false, true},
        {"excludeRead rejects a read contact", readContacts, true, false, false, false,
         false, true, false, false},
        {"excludeRead keeps an unread contact", readContacts, true, false, false, false,
         false, false, false, true},
        {"excludeArchived rejects an archived contact", archivedContacts, true, false,
         false, false, false, false, true, false},
        {"excludeArchived keeps a live contact", archivedContacts, true, false, false,
         false, false, false, false, true},
        {"include list accepts without categories", includeOnly, false, false, false,
         false, false, false, false, true},
        {"exclude list overrides the include list", includeAndExclude, false, false, false,
         false, false, false, false, false},
        {"excludeMuted overrides the include list", includedButMuted, false, false, false,
         false, true, false, false, false},
    };

    for (const MatrixRow& row : rows) {
        QVERIFY2(chatMatchesFolder(row.filter, kChatId, row.isContact, row.isGroup,
                                   row.isChannel, row.isBot, row.muted, row.read,
                                   row.archived)
                     == row.expected,
                 row.label);
    }
}

QTEST_GUILESS_MAIN(ChatFolderFiltersTest)
#include "tst_chat_folder_filters.moc"
