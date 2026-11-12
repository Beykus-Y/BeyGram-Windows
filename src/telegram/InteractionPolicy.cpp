#include "InteractionPolicy.h"

#include <QJsonValue>
#include <QStringList>

namespace beygram::telegram {
namespace {

/// Reads one Bool field of chatPermissions. A missing field keeps fallback; a
/// field that is present but not a JSON boolean is denied, so a malformed
/// payload can never be read as a grant.
bool flag(const QJsonObject& source, const QString& name, bool fallback)
{
    const QJsonValue value = source.value(name);
    if (value.isUndefined())
        return fallback;
    return value.isBool() ? value.toBool() : false;
}

/// Returns the @type of chat.type, accepting both the object shape TDLib sends
/// and a bare string from a cached or hand-built payload.
QString chatTypeName(const QJsonObject& chat)
{
    const QJsonValue type = chat.value(QStringLiteral("type"));
    const QString name =
        type.toObject().value(QStringLiteral("@type")).toString();
    return name.isEmpty() ? type.toString() : name;
}

/// Reads the media right of a chatPermissions object.
///
/// The pinned schema splits can_send_media_messages into one flag per media
/// kind; an older payload still carries the single legacy flag. When any split
/// flag is present, every kind must be granted, so one denied or missing kind
/// keeps the right false. Otherwise the legacy flag is used.
bool mediaAllowed(const QJsonObject& permissions, bool fallback)
{
    const QStringList mediaFlags{
        QStringLiteral("can_send_audios"),
        QStringLiteral("can_send_documents"),
        QStringLiteral("can_send_photos"),
        QStringLiteral("can_send_videos"),
        QStringLiteral("can_send_video_notes"),
        QStringLiteral("can_send_voice_notes"),
    };

    bool splitPresent = false;
    bool splitAllowed = true;
    for (const QString& mediaFlag : mediaFlags) {
        splitPresent = splitPresent || permissions.contains(mediaFlag);
        splitAllowed = splitAllowed && flag(permissions, mediaFlag, false);
    }

    if (splitPresent)
        return splitAllowed;
    return flag(permissions, QStringLiteral("can_send_media_messages"), fallback);
}

/// True when the message is protected by the chat or by one of the per-user
/// flags of a getMessageProperties answer.
bool hasProtectedContent(const QJsonObject& message)
{
    return message.value(QStringLiteral("has_protected_content")).toBool(false)
        || message.value(QStringLiteral("has_protected_content_by_current_user"))
               .toBool(false)
        || message.value(QStringLiteral("has_protected_content_by_other_user"))
               .toBool(false);
}

/// True when the message carries paid media, whether the payload names the
/// content type or only a marker. TDLib's own shape is messagePaidMedia with a
/// paid_media object; the other spellings keep cached and hand-built payloads
/// from being treated as free content.
bool hasPaidMedia(const QJsonObject& message)
{
    const QJsonObject content = message.value(QStringLiteral("content")).toObject();
    const QString contentType = content.value(QStringLiteral("@type")).toString();

    return contentType == QLatin1String("messagePaidMedia")
        || message.value(QStringLiteral("is_paid")).toBool(false)
        || message.value(QStringLiteral("is_paid_media")).toBool(false)
        || message.value(QStringLiteral("paid_media")).isObject()
        || content.value(QStringLiteral("is_paid")).toBool(false)
        || content.value(QStringLiteral("paid_media")).isObject();
}

} // namespace

InteractionPolicy interactionPolicyFromChat(const QJsonObject& chat)
{
    InteractionPolicy policy;

    const QString typeName = chatTypeName(chat);
    const QJsonObject type = chat.value(QStringLiteral("type")).toObject();

    policy.isChannel = typeName == QLatin1String("chatTypeSupergroup")
        && type.value(QStringLiteral("is_channel")).toBool(false);
    policy.isSecret = typeName == QLatin1String("chatTypeSecret");
    policy.isBot = chat.value(QStringLiteral("is_bot")).toBool(false)
        || type.value(QStringLiteral("is_bot")).toBool(false);
    policy.isPremiumRequired =
        chat.value(QStringLiteral("is_premium")).toBool(false);

    const QJsonValue permissionsValue = chat.value(QStringLiteral("permissions"));
    if (!permissionsValue.isObject())
        return policy;
    const QJsonObject permissions = permissionsValue.toObject();

    // The pinned schema renamed can_send_messages to can_send_basic_messages.
    // The pinned name wins and the legacy one is only a fallback, so a payload
    // that carries both follows the schema this client is built against.
    policy.canSendMessages =
        flag(permissions, QStringLiteral("can_send_basic_messages"),
             flag(permissions, QStringLiteral("can_send_messages"),
                  policy.canSendMessages));
    policy.canSendMedia = mediaAllowed(permissions, policy.canSendMedia);

    // can_add_link_previews / can_add_web_page_previews is deliberately not
    // folded into canSendMessages or canSendMedia: a denied preview only
    // changes how a text message is decorated, while text and attachments stay
    // allowed. InteractionPolicy has no link-preview field to carry it.
    policy.canSendPolls =
        flag(permissions, QStringLiteral("can_send_polls"), policy.canSendPolls);
    policy.canSendStickers =
        flag(permissions, QStringLiteral("can_send_other_messages"),
             policy.canSendStickers);

    policy.canPin =
        flag(permissions, QStringLiteral("can_pin_messages"), policy.canPin);
    policy.canReact =
        flag(permissions, QStringLiteral("can_react_to_messages"), policy.canReact);

    // can_create_topics replaced can_manage_topics; both name the member right
    // to create a topic, not the administrator right to manage existing ones.
    policy.canCreateTopics =
        flag(permissions, QStringLiteral("can_create_topics"),
             flag(permissions, QStringLiteral("can_manage_topics"),
                  policy.canCreateTopics));

    return policy;
}

InteractionPolicy interactionPolicyFromMessage(
    const QJsonObject& message, const InteractionPolicy& chatPolicy)
{
    InteractionPolicy policy = chatPolicy;

    if (hasProtectedContent(message)) {
        policy.canCopy = false;
        policy.canSave = false;
    }

    const bool secret = chatPolicy.isSecret
        || message.value(QStringLiteral("is_secret")).toBool(false);
    if (secret)
        policy.canForward = false;

    if (hasPaidMedia(message)) {
        policy.canForward = false;
        policy.canSave = false;
    }

    // A channel administrator is represented by a chat policy that still
    // allows posting; a subscriber's policy has canSendMessages false and
    // cannot reply to or edit a post.
    if (chatPolicy.isChannel && !chatPolicy.canSendMessages) {
        policy.canReply = false;
        policy.canEdit = false;
    }

    return policy;
}

QString policyDenyReason(const InteractionPolicy& policy)
{
    if (policy.isPremiumRequired)
        return QStringLiteral("premium");

    // A secret chat is private, not restricted; a closed secret chat is a
    // separate state and must not be reported as a permission denial.
    if (policy.isSecret)
        return {};

    if (!policy.canSendMessages)
        return policy.isChannel ? QStringLiteral("channel") : QStringLiteral("restricted");

    return {};
}

} // namespace beygram::telegram
