#include "NotificationRouter.h"

#include <QtGlobal>

namespace beygram::telegram {
namespace {

/// A message that is not announced: no banner, no sound, no preview text.
NotificationRoute silentRoute()
{
    NotificationRoute route;
    route.notify = false;
    route.playSound = false;
    route.showPreview = false;
    route.isMention = false;
    return route;
}

} // namespace

NotificationRoute routeNotification(bool chatMuted, bool isSecret, bool hasProtectedContent,
                                    bool isOutgoing, bool isCurrentChat, bool isMention,
                                    bool appForeground, bool previewEnabled,
                                    bool notifyMutedChats)
{
    // Own messages, the chat being read, secret chats and protected content are
    // never announced, whatever the notification settings say.
    if (isOutgoing || isCurrentChat || isSecret || hasProtectedContent)
        return silentRoute();

    // A muted chat notifies only when muted notifications are allowed, and a
    // mention does not open that gate by itself.
    if (chatMuted && !notifyMutedChats)
        return silentRoute();

    // The window being focused is not a reason to stay quiet: only the chat the
    // user is reading suppresses a notification, and that is handled above.
    Q_UNUSED(appForeground);

    NotificationRoute route;
    route.notify = true;
    // A muted chat that still notifies stays silent.
    route.playSound = !chatMuted;
    route.showPreview = previewEnabled;
    route.isMention = isMention;
    return route;
}

} // namespace beygram::telegram
