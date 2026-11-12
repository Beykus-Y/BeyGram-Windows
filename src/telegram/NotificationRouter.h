#pragma once

namespace beygram::telegram {

/// What one incoming message is allowed to do to the user.
///
/// `notify` is the gate: when it is false the message is dropped silently and
/// every other member is false as well, so a caller never has to re-check the
/// inputs before showing a banner, playing a sound or filling in a preview.
/// `isMention` is reported only for a message that actually notifies, because a
/// mention label on a suppressed message would never be seen.
struct NotificationRoute {
    bool notify = false;
    bool playSound = true;
    bool showPreview = true;
    bool isMention = false;
};

/// Decides how a single incoming message is announced.
///
/// Inputs, in the order the decision applies them:
///   - isOutgoing: the user sent the message, so there is nothing to announce.
///   - isCurrentChat: the user is reading that chat right now.
///   - isSecret: secret chats never raise notifications.
///   - hasProtectedContent: protected content must not surface outside the chat.
///     The first four suppress the notification unconditionally.
///   - chatMuted: the chat is muted. It still notifies when notifyMutedChats is
///     true ("notify muted chats"), but silently, because the mute asks for no
///     sound. A mention is not an exception to this gate: mention overrides a
///     mute only where muted notifications are allowed at all.
///   - isMention: the message mentions the user. Reported through isMention
///     when the message notifies, and it never widens the rules above.
///   - appForeground: the window is focused. Deliberately does not suppress:
///     only the chat being read suppresses, so a message in another chat is
///     still announced while the app is in front.
///   - previewEnabled: message text may be shown in a notification. A false
///     value yields a notification without content rather than no notification.
///   - notifyMutedChats: the user allowed notifications from muted chats.
///
/// The function is pure: no I/O, no logging, no side effects.
[[nodiscard]] NotificationRoute routeNotification(bool chatMuted, bool isSecret,
                                                  bool hasProtectedContent, bool isOutgoing,
                                                  bool isCurrentChat, bool isMention,
                                                  bool appForeground, bool previewEnabled,
                                                  bool notifyMutedChats);

} // namespace beygram::telegram
