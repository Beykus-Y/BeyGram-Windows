#include "ChatPermissions.h"

#include <QJsonValue>
#include <QString>
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
    return value.toBool(false);
}

/// Returns the object that carries the permission flags: the nested
/// chatPermissions of a chat or supergroup, or the object itself when it
/// already is a chatPermissions.
QJsonObject permissionSource(const QJsonObject& container)
{
    const QJsonValue nested = container.value(QStringLiteral("permissions"));
    if (nested.isObject())
        return nested.toObject();
    return container;
}

ChatPermissions parsePermissions(const QJsonObject& container)
{
    const QJsonObject source = permissionSource(container);
    ChatPermissions permissions;

    // The pinned schema renamed can_send_messages to can_send_basic_messages.
    // The pinned name wins and the legacy one is only a fallback, so a payload
    // that carries both follows the schema this client is built against.
    permissions.canSendMessages =
        flag(source, QStringLiteral("can_send_basic_messages"),
             flag(source, QStringLiteral("can_send_messages"), false));

    // The pinned schema splits the old can_send_media_messages into one flag per
    // media kind. Attaching media is offered only when every kind is granted, so
    // a single false or missing kind keeps canSendMedia false.
    const QStringList mediaFlags{
        QStringLiteral("can_send_audios"),
        QStringLiteral("can_send_documents"),
        QStringLiteral("can_send_photos"),
        QStringLiteral("can_send_videos"),
        QStringLiteral("can_send_video_notes"),
        QStringLiteral("can_send_voice_notes"),
    };
    bool mediaSplitPresent = false;
    bool mediaAllowed = true;
    for (const QString& mediaFlag : mediaFlags) {
        mediaSplitPresent = mediaSplitPresent || source.contains(mediaFlag);
        mediaAllowed = mediaAllowed && flag(source, mediaFlag, false);
    }
    permissions.canSendMedia =
        mediaSplitPresent ? mediaAllowed
                          : flag(source, QStringLiteral("can_send_media_messages"), false);

    permissions.canSendPolls = flag(source, QStringLiteral("can_send_polls"), false);
    permissions.canSendOther =
        flag(source, QStringLiteral("can_send_other_messages"), false);

    // Link previews are granted by default; only an explicit flag takes them
    // away. The pinned name is can_add_link_previews.
    permissions.canAddWebPreviews =
        flag(source, QStringLiteral("can_add_link_previews"),
             flag(source, QStringLiteral("can_add_web_page_previews"),
                  permissions.canAddWebPreviews));

    permissions.canChangeInfo = flag(source, QStringLiteral("can_change_info"), false);
    permissions.canInviteUsers = flag(source, QStringLiteral("can_invite_users"), false);
    permissions.canPinMessages = flag(source, QStringLiteral("can_pin_messages"), false);

    // Topics were renamed from can_manage_topics to can_create_topics.
    permissions.canManageTopics =
        flag(source, QStringLiteral("can_create_topics"),
             flag(source, QStringLiteral("can_manage_topics"), false));

    return permissions;
}

} // namespace

ChatPermissions permissionsFromChat(const QJsonObject& chat)
{
    return parsePermissions(chat);
}

ChatPermissions permissionsFromSupergroup(const QJsonObject& supergroup)
{
    // A supergroup object carries no permissions of its own in the pinned
    // schema, so this accepts whatever the caller has: the same nested shape as
    // a chat, a bare chatPermissions, or nothing at all (the defaults).
    return parsePermissions(supergroup);
}

} // namespace beygram::telegram
