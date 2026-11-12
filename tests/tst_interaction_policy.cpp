#include "telegram/InteractionPolicy.h"

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

} // namespace

class InteractionPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsDescribeAnOrdinaryPrivateChat();
    void chatWithoutPermissionsKeepsDefaults();
    void deniedPermissionsWithdrawSendActions();
    void allowedPermissionsGrantSendActions();
    void partialPermissionsKeepDefaults();
    void nonBooleanPermissionsAreDenied();
    void legacyPermissionNamesAreAccepted();
    void pinnedMediaFlagsNeedEveryKind();
    void linkPreviewPermissionDoesNotBlockTextOrMedia();
    void detectsChannelSecretBotAndPremium();
    void protectedContentWithdrawsCopyAndSave();
    void perUserProtectedFlagsAlsoWithdrawCopyAndSave();
    void secretChatWithdrawsForwardOnly();
    void paidMediaWithdrawsForwardAndSave();
    void channelWithdrawsReplyAndEditFromSubscribers();
    void channelAdministratorKeepsReplyAndEdit();
    void ordinaryMessageReturnsChatPolicyUnchanged();
    void denyReasonPriority();
};

void InteractionPolicyTest::defaultsDescribeAnOrdinaryPrivateChat()
{
    const InteractionPolicy policy;

    QVERIFY(policy.canSendMessages);
    QVERIFY(policy.canSendMedia);
    QVERIFY(policy.canSendPolls);
    QVERIFY(policy.canSendStickers);

    QVERIFY(policy.canReply);
    QVERIFY(policy.canForward);
    QVERIFY(policy.canCopy);
    QVERIFY(policy.canSave);

    QVERIFY(policy.canReact);
    QVERIFY(!policy.canEdit);
    QVERIFY(policy.canDelete);
    QVERIFY(policy.canPin);

    QVERIFY(policy.canCreateTopics);
    QVERIFY(!policy.canManageTopics);

    QVERIFY(!policy.isChannel);
    QVERIFY(!policy.isSecret);
    QVERIFY(!policy.isBot);
    QVERIFY(!policy.isPremiumRequired);
}

void InteractionPolicyTest::chatWithoutPermissionsKeepsDefaults()
{
    const QJsonObject chat = parseObject(R"({
        "@type": "chat",
        "id": 1,
        "title": "Private",
        "type": { "@type": "chatTypePrivate", "user_id": 2 }
    })");

    const InteractionPolicy policy = interactionPolicyFromChat(chat);
    QVERIFY(policy.canSendMessages);
    QVERIFY(policy.canSendMedia);
    QVERIFY(policy.canSendPolls);
    QVERIFY(policy.canSendStickers);
    QVERIFY(policy.canPin);
    QVERIFY(policy.canReact);
    QVERIFY(policy.canCreateTopics);
    QVERIFY(!policy.isChannel);
    QVERIFY(!policy.isSecret);
    QVERIFY(!policy.isBot);
    QVERIFY(!policy.isPremiumRequired);

    // A null permissions field is the same as an absent one: a private chat
    // has no restriction object, so the defaults stand.
    const QJsonObject nullPermissions = parseObject(R"({
        "@type": "chat",
        "id": 2,
        "type": { "@type": "chatTypePrivate", "user_id": 3 },
        "permissions": null
    })");
    QVERIFY(interactionPolicyFromChat(nullPermissions).canSendMessages);
}

void InteractionPolicyTest::deniedPermissionsWithdrawSendActions()
{
    const QJsonObject chat = parseObject(R"({
        "@type": "chat",
        "id": 10,
        "type": { "@type": "chatTypeBasicGroup", "basic_group_id": 3 },
        "permissions": {
            "@type": "chatPermissions",
            "can_send_basic_messages": false,
            "can_send_media_messages": false,
            "can_send_polls": false,
            "can_send_other_messages": false,
            "can_add_web_page_previews": false,
            "can_pin_messages": false,
            "can_react_to_messages": false,
            "can_create_topics": false
        }
    })");

    const InteractionPolicy policy = interactionPolicyFromChat(chat);
    QVERIFY(!policy.canSendMessages);
    QVERIFY(!policy.canSendMedia);
    QVERIFY(!policy.canSendPolls);
    QVERIFY(!policy.canSendStickers);
    QVERIFY(!policy.canPin);
    QVERIFY(!policy.canReact);
    QVERIFY(!policy.canCreateTopics);

    // The chat-level permission object does not decide reply or edit; those
    // are narrowed per message by interactionPolicyFromMessage.
    QVERIFY(policy.canReply);
    QVERIFY(policy.canForward);
    QVERIFY(policy.canCopy);
    QVERIFY(policy.canSave);
    QVERIFY(policy.canDelete);
    QVERIFY(!policy.canEdit);
    QVERIFY(!policy.canManageTopics);
}

void InteractionPolicyTest::allowedPermissionsGrantSendActions()
{
    const QJsonObject chat = parseObject(R"({
        "@type": "chat",
        "id": 11,
        "type": { "@type": "chatTypeSupergroup", "supergroup_id": 4, "is_channel": false },
        "permissions": {
            "@type": "chatPermissions",
            "can_send_basic_messages": true,
            "can_send_media_messages": true,
            "can_send_polls": true,
            "can_send_other_messages": true,
            "can_add_web_page_previews": true,
            "can_pin_messages": true,
            "can_react_to_messages": true,
            "can_create_topics": true
        }
    })");

    const InteractionPolicy policy = interactionPolicyFromChat(chat);
    QVERIFY(policy.canSendMessages);
    QVERIFY(policy.canSendMedia);
    QVERIFY(policy.canSendPolls);
    QVERIFY(policy.canSendStickers);
    QVERIFY(policy.canPin);
    QVERIFY(policy.canReact);
    QVERIFY(policy.canCreateTopics);
}

void InteractionPolicyTest::partialPermissionsKeepDefaults()
{
    // Only the text right is denied. The absent flags are unknown, not false,
    // so they keep the permissive defaults of the struct.
    const QJsonObject chat = parseObject(R"({
        "permissions": {
            "can_send_basic_messages": false
        }
    })");

    const InteractionPolicy policy = interactionPolicyFromChat(chat);
    QVERIFY(!policy.canSendMessages);
    QVERIFY(policy.canSendMedia);
    QVERIFY(policy.canSendPolls);
    QVERIFY(policy.canSendStickers);
    QVERIFY(policy.canPin);
    QVERIFY(policy.canReact);
    QVERIFY(policy.canCreateTopics);
}

void InteractionPolicyTest::nonBooleanPermissionsAreDenied()
{
    // A flag that is present but not a JSON boolean is malformed; it must be
    // read as a denial rather than granting the right.
    const QJsonObject chat = parseObject(R"({
        "permissions": {
            "can_send_basic_messages": "yes",
            "can_send_media_messages": 1,
            "can_send_polls": null
        }
    })");

    const InteractionPolicy policy = interactionPolicyFromChat(chat);
    QVERIFY(!policy.canSendMessages);
    QVERIFY(!policy.canSendMedia);
    QVERIFY(!policy.canSendPolls);
    // Absent flags still keep their defaults.
    QVERIFY(policy.canSendStickers);
}

void InteractionPolicyTest::legacyPermissionNamesAreAccepted()
{
    // A payload cached by an older client uses the pre-split names.
    const QJsonObject chat = parseObject(R"({
        "permissions": {
            "can_send_messages": false,
            "can_manage_topics": false
        }
    })");
    const InteractionPolicy policy = interactionPolicyFromChat(chat);
    QVERIFY(!policy.canSendMessages);
    QVERIFY(!policy.canCreateTopics);

    const QJsonObject media = parseObject(R"({
        "permissions": { "can_send_media_messages": true }
    })");
    QVERIFY(interactionPolicyFromChat(media).canSendMedia);
}

void InteractionPolicyTest::pinnedMediaFlagsNeedEveryKind()
{
    const QJsonObject allKinds = parseObject(R"({
        "permissions": {
            "can_send_audios": true,
            "can_send_documents": true,
            "can_send_photos": true,
            "can_send_videos": true,
            "can_send_video_notes": true,
            "can_send_voice_notes": true
        }
    })");
    QVERIFY(interactionPolicyFromChat(allKinds).canSendMedia);

    // One denied kind is enough to keep the attach action false.
    const QJsonObject oneDenied = parseObject(R"({
        "permissions": {
            "can_send_audios": true,
            "can_send_documents": true,
            "can_send_photos": false,
            "can_send_videos": true,
            "can_send_video_notes": true,
            "can_send_voice_notes": true
        }
    })");
    QVERIFY(!interactionPolicyFromChat(oneDenied).canSendMedia);
}

void InteractionPolicyTest::linkPreviewPermissionDoesNotBlockTextOrMedia()
{
    // Link previews decorate a text message. A denied preview must not hide
    // the composer or attachments, which Telegram still allows.
    const QJsonObject legacy = parseObject(R"({
        "permissions": {
            "can_send_basic_messages": true,
            "can_send_media_messages": true,
            "can_send_polls": true,
            "can_send_other_messages": true,
            "can_add_web_page_previews": false
        }
    })");
    const InteractionPolicy legacyPolicy = interactionPolicyFromChat(legacy);
    QVERIFY(legacyPolicy.canSendMessages);
    QVERIFY(legacyPolicy.canSendMedia);
    QVERIFY(legacyPolicy.canSendPolls);
    QVERIFY(legacyPolicy.canSendStickers);

    const QJsonObject pinned = parseObject(R"({
        "permissions": {
            "can_send_basic_messages": true,
            "can_send_audios": true,
            "can_send_documents": true,
            "can_send_photos": true,
            "can_send_videos": true,
            "can_send_video_notes": true,
            "can_send_voice_notes": true,
            "can_add_link_previews": false
        }
    })");
    const InteractionPolicy pinnedPolicy = interactionPolicyFromChat(pinned);
    QVERIFY(pinnedPolicy.canSendMessages);
    QVERIFY(pinnedPolicy.canSendMedia);
}

void InteractionPolicyTest::detectsChannelSecretBotAndPremium()
{
    const InteractionPolicy channel = interactionPolicyFromChat(parseObject(R"({
        "@type": "chat",
        "id": 20,
        "type": { "@type": "chatTypeSupergroup", "supergroup_id": 5, "is_channel": true },
        "is_premium": true
    })"));
    QVERIFY(channel.isChannel);
    QVERIFY(!channel.isSecret);
    QVERIFY(!channel.isBot);
    QVERIFY(channel.isPremiumRequired);

    const InteractionPolicy group = interactionPolicyFromChat(parseObject(R"({
        "type": { "@type": "chatTypeSupergroup", "supergroup_id": 6, "is_channel": false }
    })"));
    QVERIFY(!group.isChannel);

    const InteractionPolicy secret = interactionPolicyFromChat(parseObject(R"({
        "type": { "@type": "chatTypeSecret", "secret_chat_id": 7, "user_id": 8 }
    })"));
    QVERIFY(secret.isSecret);
    QVERIFY(!secret.isChannel);

    const InteractionPolicy bot = interactionPolicyFromChat(parseObject(R"({
        "type": { "@type": "chatTypePrivate", "user_id": 9 },
        "is_bot": true
    })"));
    QVERIFY(bot.isBot);
    QVERIFY(!bot.isPremiumRequired);

    const InteractionPolicy nestedBot = interactionPolicyFromChat(parseObject(R"({
        "type": { "@type": "chatTypePrivate", "user_id": 9, "is_bot": true }
    })"));
    QVERIFY(nestedBot.isBot);

    // A malformed is_premium is not a requirement.
    const InteractionPolicy malformed = interactionPolicyFromChat(parseObject(R"({
        "is_premium": "yes"
    })"));
    QVERIFY(!malformed.isPremiumRequired);
}

void InteractionPolicyTest::protectedContentWithdrawsCopyAndSave()
{
    const InteractionPolicy chatPolicy;
    const QJsonObject message = parseObject(R"({
        "@type": "message",
        "id": 30,
        "has_protected_content": true,
        "content": {
            "@type": "messageText",
            "text": { "@type": "formattedText", "text": "hello" }
        }
    })");

    const InteractionPolicy policy =
        interactionPolicyFromMessage(message, chatPolicy);
    QVERIFY(!policy.canCopy);
    QVERIFY(!policy.canSave);
    // The requested rule withdraws copy and save only.
    QVERIFY(policy.canForward);
    QVERIFY(policy.canReply);
    QVERIFY(policy.canDelete);
}

void InteractionPolicyTest::perUserProtectedFlagsAlsoWithdrawCopyAndSave()
{
    const QJsonObject currentUser = parseObject(R"({
        "@type": "message",
        "id": 31,
        "has_protected_content_by_current_user": true
    })");
    const InteractionPolicy fromCurrentUser =
        interactionPolicyFromMessage(currentUser, InteractionPolicy{});
    QVERIFY(!fromCurrentUser.canCopy);
    QVERIFY(!fromCurrentUser.canSave);

    const QJsonObject otherUser = parseObject(R"({
        "@type": "message",
        "id": 32,
        "has_protected_content_by_other_user": true
    })");
    const InteractionPolicy fromOtherUser =
        interactionPolicyFromMessage(otherUser, InteractionPolicy{});
    QVERIFY(!fromOtherUser.canCopy);
    QVERIFY(!fromOtherUser.canSave);
}

void InteractionPolicyTest::secretChatWithdrawsForwardOnly()
{
    InteractionPolicy chatPolicy;
    chatPolicy.isSecret = true;

    const InteractionPolicy policy = interactionPolicyFromMessage(
        parseObject(R"({ "@type": "message", "id": 40 })"), chatPolicy);
    QVERIFY(!policy.canForward);
    QVERIFY(policy.canCopy);
    QVERIFY(policy.canSave);
    QVERIFY(policy.canReply);
    QVERIFY(policy.canDelete);

    // A message-level marker has the same effect when the caller has only the
    // message and not the chat kind.
    const InteractionPolicy marked = interactionPolicyFromMessage(
        parseObject(R"({ "@type": "message", "id": 41, "is_secret": true })"),
        InteractionPolicy{});
    QVERIFY(!marked.canForward);
}

void InteractionPolicyTest::paidMediaWithdrawsForwardAndSave()
{
    const QJsonObject content = parseObject(R"({
        "@type": "message",
        "id": 50,
        "content": {
            "@type": "messagePaidMedia",
            "paid_media": { "@type": "paidMedia" }
        }
    })");
    const InteractionPolicy fromContent =
        interactionPolicyFromMessage(content, InteractionPolicy{});
    QVERIFY(!fromContent.canForward);
    QVERIFY(!fromContent.canSave);
    QVERIFY(fromContent.canCopy);

    const QJsonObject marker = parseObject(R"({
        "@type": "message",
        "id": 51,
        "is_paid": true
    })");
    const InteractionPolicy fromMarker =
        interactionPolicyFromMessage(marker, InteractionPolicy{});
    QVERIFY(!fromMarker.canForward);
    QVERIFY(!fromMarker.canSave);

    const QJsonObject paidObject = parseObject(R"({
        "@type": "message",
        "id": 52,
        "paid_media": {}
    })");
    const InteractionPolicy fromObject =
        interactionPolicyFromMessage(paidObject, InteractionPolicy{});
    QVERIFY(!fromObject.canForward);
    QVERIFY(!fromObject.canSave);

    // Protected paid media combines both rules.
    const QJsonObject combined = parseObject(R"({
        "@type": "message",
        "id": 53,
        "has_protected_content": true,
        "content": { "@type": "messagePaidMedia" }
    })");
    const InteractionPolicy fromCombined =
        interactionPolicyFromMessage(combined, InteractionPolicy{});
    QVERIFY(!fromCombined.canCopy);
    QVERIFY(!fromCombined.canSave);
    QVERIFY(!fromCombined.canForward);
}

void InteractionPolicyTest::channelWithdrawsReplyAndEditFromSubscribers()
{
    InteractionPolicy chatPolicy;
    chatPolicy.isChannel = true;
    chatPolicy.canSendMessages = false;
    chatPolicy.canReply = true;
    chatPolicy.canEdit = true;

    const InteractionPolicy policy = interactionPolicyFromMessage(
        parseObject(R"({ "@type": "message", "id": 60 })"), chatPolicy);
    QVERIFY(!policy.canReply);
    QVERIFY(!policy.canEdit);

    // A channel post can still be forwarded, copied and saved by a subscriber.
    QVERIFY(policy.canForward);
    QVERIFY(policy.canCopy);
    QVERIFY(policy.canSave);
}

void InteractionPolicyTest::channelAdministratorKeepsReplyAndEdit()
{
    InteractionPolicy chatPolicy;
    chatPolicy.isChannel = true;
    chatPolicy.canSendMessages = true;
    chatPolicy.canReply = true;
    chatPolicy.canEdit = true;

    const InteractionPolicy policy = interactionPolicyFromMessage(
        parseObject(R"({ "@type": "message", "id": 61 })"), chatPolicy);
    QVERIFY(policy.canReply);
    QVERIFY(policy.canEdit);
}

void InteractionPolicyTest::ordinaryMessageReturnsChatPolicyUnchanged()
{
    InteractionPolicy chatPolicy;
    chatPolicy.canSendMessages = false;
    chatPolicy.canReply = false;
    chatPolicy.canForward = false;
    chatPolicy.canCopy = false;
    chatPolicy.canSave = false;
    chatPolicy.canEdit = true;
    chatPolicy.canPin = false;
    chatPolicy.canReact = false;

    const InteractionPolicy policy = interactionPolicyFromMessage(
        parseObject(R"({ "@type": "message", "id": 70 })"), chatPolicy);
    QVERIFY(!policy.canSendMessages);
    QVERIFY(!policy.canReply);
    QVERIFY(!policy.canForward);
    QVERIFY(!policy.canCopy);
    QVERIFY(!policy.canSave);
    QVERIFY(policy.canEdit);
    QVERIFY(!policy.canPin);
    QVERIFY(!policy.canReact);
}

void InteractionPolicyTest::denyReasonPriority()
{
    {
        const InteractionPolicy policy;
        QVERIFY(policyDenyReason(policy).isEmpty());
    }
    {
        InteractionPolicy policy;
        policy.canSendMessages = false;
        QCOMPARE(policyDenyReason(policy), QStringLiteral("restricted"));
    }
    {
        InteractionPolicy policy;
        policy.isChannel = true;
        policy.canSendMessages = false;
        QCOMPARE(policyDenyReason(policy), QStringLiteral("channel"));
    }
    {
        // A channel administrator still has the posting right.
        InteractionPolicy policy;
        policy.isChannel = true;
        QVERIFY(policyDenyReason(policy).isEmpty());
    }
    {
        InteractionPolicy policy;
        policy.isPremiumRequired = true;
        QCOMPARE(policyDenyReason(policy), QStringLiteral("premium"));
    }
    {
        // Premium wins over a channel restriction.
        InteractionPolicy policy;
        policy.isPremiumRequired = true;
        policy.isChannel = true;
        policy.canSendMessages = false;
        QCOMPARE(policyDenyReason(policy), QStringLiteral("premium"));
    }
    {
        // Secret chats never produce a denial key.
        InteractionPolicy policy;
        policy.isSecret = true;
        QVERIFY(policyDenyReason(policy).isEmpty());
    }
    {
        InteractionPolicy policy;
        policy.isSecret = true;
        policy.canSendMessages = false;
        QVERIFY(policyDenyReason(policy).isEmpty());
    }
    {
        // A media-only restriction does not close the composer.
        InteractionPolicy policy;
        policy.canSendMedia = false;
        policy.canSendPolls = false;
        policy.canSendStickers = false;
        QVERIFY(policyDenyReason(policy).isEmpty());
    }
}

QTEST_GUILESS_MAIN(InteractionPolicyTest)
#include "tst_interaction_policy.moc"
