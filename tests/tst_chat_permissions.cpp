#include "telegram/ChatPermissions.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

// The defaults are asserted field by field because ChatPermissions has no
// equality operator; a new field must be added here on purpose.
void verifyDefaults(const ChatPermissions& permissions)
{
    QVERIFY(!permissions.canSendMessages);
    QVERIFY(!permissions.canSendMedia);
    QVERIFY(!permissions.canSendPolls);
    QVERIFY(!permissions.canSendOther);
    // Link previews are the one right Telegram grants unless it is revoked.
    QVERIFY(permissions.canAddWebPreviews);
    QVERIFY(!permissions.canChangeInfo);
    QVERIFY(!permissions.canInviteUsers);
    QVERIFY(!permissions.canPinMessages);
    QVERIFY(!permissions.canManageTopics);
}

} // namespace

class ChatPermissionsTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsAreRestrictive();
    void parsesFullLegacyPermissions();
    void parsesFullPinnedPermissions();
    void pinnedNamesWinOverLegacy();
    void partialPermissionsKeepDefaults();
    void mediaNeedsEveryMediaKind();
    void missingPermissionsUseDefaults();
    void nonBooleanFlagsAreDenied();
    void supergroupReadsEmbeddedPermissions();
    void supergroupWithoutPermissionsUsesDefaults();
    void acceptsBarePermissionsObject();
};

void ChatPermissionsTest::defaultsAreRestrictive()
{
    verifyDefaults(ChatPermissions{});
}

void ChatPermissionsTest::parsesFullLegacyPermissions()
{
    // Shape used by older revisions, still cached by existing clients.
    const QJsonObject chat = parseObject(R"({
        "@type": "chat",
        "id": 42,
        "title": "Legacy group",
        "permissions": {
            "@type": "chatPermissions",
            "can_send_messages": true,
            "can_send_media_messages": true,
            "can_send_polls": false,
            "can_send_other_messages": true,
            "can_add_web_page_previews": false,
            "can_change_info": false,
            "can_invite_users": true,
            "can_pin_messages": false,
            "can_manage_topics": true
        }
    })");

    const ChatPermissions permissions = permissionsFromChat(chat);
    QVERIFY(permissions.canSendMessages);
    QVERIFY(permissions.canSendMedia);
    QVERIFY(!permissions.canSendPolls);
    QVERIFY(permissions.canSendOther);
    QVERIFY(!permissions.canAddWebPreviews);
    QVERIFY(!permissions.canChangeInfo);
    QVERIFY(permissions.canInviteUsers);
    QVERIFY(!permissions.canPinMessages);
    QVERIFY(permissions.canManageTopics);
}

void ChatPermissionsTest::parsesFullPinnedPermissions()
{
    // Shape of the pinned td_api.tl: one flag per media kind, link previews and
    // topics renamed.
    const QJsonObject chat = parseObject(R"({
        "@type": "chat",
        "id": 43,
        "title": "Pinned group",
        "permissions": {
            "@type": "chatPermissions",
            "can_send_basic_messages": true,
            "can_send_audios": true,
            "can_send_documents": true,
            "can_send_photos": true,
            "can_send_videos": true,
            "can_send_video_notes": true,
            "can_send_voice_notes": true,
            "can_send_polls": true,
            "can_send_other_messages": true,
            "can_add_link_previews": false,
            "can_react_to_messages": true,
            "can_edit_tag": false,
            "can_change_info": true,
            "can_invite_users": true,
            "can_pin_messages": true,
            "can_create_topics": false
        }
    })");

    const ChatPermissions permissions = permissionsFromChat(chat);
    QVERIFY(permissions.canSendMessages);
    QVERIFY(permissions.canSendMedia);
    QVERIFY(permissions.canSendPolls);
    QVERIFY(permissions.canSendOther);
    QVERIFY(!permissions.canAddWebPreviews);
    QVERIFY(permissions.canChangeInfo);
    QVERIFY(permissions.canInviteUsers);
    QVERIFY(permissions.canPinMessages);
    QVERIFY(!permissions.canManageTopics);
}

void ChatPermissionsTest::pinnedNamesWinOverLegacy()
{
    // A payload carrying both spellings follows the pinned schema; the legacy
    // field is only a fallback for payloads that predate the split.
    const QJsonObject chat = parseObject(R"({
        "@type": "chat",
        "permissions": {
            "can_send_messages": true,
            "can_send_basic_messages": false,
            "can_send_media_messages": true,
            "can_send_audios": false,
            "can_send_documents": true,
            "can_send_photos": true,
            "can_send_videos": true,
            "can_send_video_notes": true,
            "can_send_voice_notes": true,
            "can_add_web_page_previews": false,
            "can_add_link_previews": true,
            "can_manage_topics": false,
            "can_create_topics": true
        }
    })");

    const ChatPermissions permissions = permissionsFromChat(chat);
    QVERIFY(!permissions.canSendMessages);
    QVERIFY(!permissions.canSendMedia);
    QVERIFY(permissions.canAddWebPreviews);
    QVERIFY(permissions.canManageTopics);
}

void ChatPermissionsTest::partialPermissionsKeepDefaults()
{
    const ChatPermissions legacyOnly =
        permissionsFromChat(parseObject(R"({
            "@type": "chat",
            "permissions": { "can_send_messages": true }
        })"));
    QVERIFY(legacyOnly.canSendMessages);
    QVERIFY(!legacyOnly.canSendMedia);
    QVERIFY(!legacyOnly.canSendPolls);
    QVERIFY(!legacyOnly.canSendOther);
    QVERIFY(legacyOnly.canAddWebPreviews);
    QVERIFY(!legacyOnly.canChangeInfo);
    QVERIFY(!legacyOnly.canInviteUsers);
    QVERIFY(!legacyOnly.canPinMessages);
    QVERIFY(!legacyOnly.canManageTopics);

    const ChatPermissions pinnedOnly =
        permissionsFromChat(parseObject(R"({
            "@type": "chat",
            "permissions": { "can_send_basic_messages": true, "can_invite_users": true }
        })"));
    QVERIFY(pinnedOnly.canSendMessages);
    QVERIFY(!pinnedOnly.canSendMedia);
    QVERIFY(pinnedOnly.canAddWebPreviews);
    QVERIFY(pinnedOnly.canInviteUsers);
    QVERIFY(!pinnedOnly.canChangeInfo);
    QVERIFY(!pinnedOnly.canPinMessages);
}

void ChatPermissionsTest::mediaNeedsEveryMediaKind()
{
    const QJsonObject allKinds = parseObject(R"({
        "can_send_audios": true,
        "can_send_documents": true,
        "can_send_photos": true,
        "can_send_videos": true,
        "can_send_video_notes": true,
        "can_send_voice_notes": true
    })");
    QVERIFY(permissionsFromChat(allKinds).canSendMedia);

    // One denied kind is enough to keep the attach action away.
    const QJsonObject noVoice = parseObject(R"({
        "can_send_audios": true,
        "can_send_documents": true,
        "can_send_photos": true,
        "can_send_videos": true,
        "can_send_video_notes": true,
        "can_send_voice_notes": false
    })");
    QVERIFY(!permissionsFromChat(noVoice).canSendMedia);

    // A partial split cannot grant media: the missing kinds are not allowed.
    const QJsonObject oneKind = parseObject(R"({ "can_send_photos": true })");
    QVERIFY(!permissionsFromChat(oneKind).canSendMedia);

    // With no split field at all the legacy flag decides.
    const QJsonObject legacyOnly = parseObject(R"({ "can_send_media_messages": true })");
    QVERIFY(permissionsFromChat(legacyOnly).canSendMedia);

    const QJsonObject legacyDenied = parseObject(R"({ "can_send_media_messages": false })");
    QVERIFY(!permissionsFromChat(legacyDenied).canSendMedia);
}

void ChatPermissionsTest::missingPermissionsUseDefaults()
{
    verifyDefaults(permissionsFromChat(QJsonObject{}));
    verifyDefaults(permissionsFromChat(parseObject(R"({ "@type": "chat", "id": 1 })")));
    verifyDefaults(permissionsFromChat(
        parseObject(R"({ "@type": "chat", "id": 1, "permissions": null })")));
    verifyDefaults(permissionsFromChat(
        parseObject(R"({ "@type": "chat", "id": 1, "permissions": "read-only" })")));
    verifyDefaults(permissionsFromChat(
        parseObject(R"({ "@type": "chat", "id": 1, "permissions": 7 })")));
    // An empty chatPermissions object is a chat that grants nothing new.
    verifyDefaults(permissionsFromChat(
        parseObject(R"({ "@type": "chat", "permissions": {} })")));
}

void ChatPermissionsTest::nonBooleanFlagsAreDenied()
{
    // Malformed values never grant a right; only an absent flag keeps a default.
    const QJsonObject chat = parseObject(R"({
        "@type": "chat",
        "permissions": {
            "can_send_messages": "true",
            "can_send_media_messages": null,
            "can_send_polls": "yes",
            "can_send_other_messages": 1,
            "can_add_web_page_previews": 0,
            "can_invite_users": "true"
        }
    })");

    const ChatPermissions permissions = permissionsFromChat(chat);
    QVERIFY(!permissions.canSendMessages);
    QVERIFY(!permissions.canSendMedia);
    QVERIFY(!permissions.canSendPolls);
    QVERIFY(!permissions.canSendOther);
    QVERIFY(!permissions.canAddWebPreviews);
    QVERIFY(!permissions.canInviteUsers);
    QVERIFY(!permissions.canChangeInfo);
    QVERIFY(!permissions.canPinMessages);
    QVERIFY(!permissions.canManageTopics);
}

void ChatPermissionsTest::supergroupReadsEmbeddedPermissions()
{
    // A supergroup merged with the chat that owns it carries chat.permissions.
    const QJsonObject supergroup = parseObject(R"({
        "@type": "supergroup",
        "id": -1001234567890,
        "permissions": {
            "@type": "chatPermissions",
            "can_send_messages": true,
            "can_send_media_messages": true,
            "can_send_polls": true,
            "can_send_other_messages": false,
            "can_add_web_page_previews": false,
            "can_change_info": true,
            "can_invite_users": true,
            "can_pin_messages": true,
            "can_manage_topics": true
        }
    })");

    const ChatPermissions permissions = permissionsFromSupergroup(supergroup);
    QVERIFY(permissions.canSendMessages);
    QVERIFY(permissions.canSendMedia);
    QVERIFY(permissions.canSendPolls);
    QVERIFY(!permissions.canSendOther);
    QVERIFY(!permissions.canAddWebPreviews);
    QVERIFY(permissions.canChangeInfo);
    QVERIFY(permissions.canInviteUsers);
    QVERIFY(permissions.canPinMessages);
    QVERIFY(permissions.canManageTopics);
}

void ChatPermissionsTest::supergroupWithoutPermissionsUsesDefaults()
{
    // The pinned schema gives supergroup no permissions field of its own.
    verifyDefaults(permissionsFromSupergroup(QJsonObject{}));
    verifyDefaults(permissionsFromSupergroup(parseObject(R"({
        "@type": "supergroup",
        "id": -1001234567890,
        "is_channel": false,
        "is_forum": true
    })")));
    verifyDefaults(permissionsFromSupergroup(parseObject(R"({
        "@type": "supergroup",
        "id": -1001234567890,
        "permissions": null
    })")));
}

void ChatPermissionsTest::acceptsBarePermissionsObject()
{
    // A chatPermissions object can be parsed without a wrapping chat.
    const QJsonObject bare = parseObject(R"({
        "@type": "chatPermissions",
        "can_send_basic_messages": true,
        "can_send_audios": true,
        "can_send_documents": true,
        "can_send_photos": true,
        "can_send_videos": true,
        "can_send_video_notes": true,
        "can_send_voice_notes": true,
        "can_add_link_previews": false,
        "can_create_topics": true
    })");

    const ChatPermissions fromSupergroup = permissionsFromSupergroup(bare);
    QVERIFY(fromSupergroup.canSendMessages);
    QVERIFY(fromSupergroup.canSendMedia);
    QVERIFY(!fromSupergroup.canAddWebPreviews);
    QVERIFY(fromSupergroup.canManageTopics);
    QVERIFY(!fromSupergroup.canSendPolls);

    const ChatPermissions fromChat = permissionsFromChat(bare);
    QVERIFY(fromChat.canSendMessages);
    QVERIFY(fromChat.canSendMedia);
    QVERIFY(!fromChat.canAddWebPreviews);
    QVERIFY(fromChat.canManageTopics);
}

QTEST_GUILESS_MAIN(ChatPermissionsTest)
#include "tst_chat_permissions.moc"
