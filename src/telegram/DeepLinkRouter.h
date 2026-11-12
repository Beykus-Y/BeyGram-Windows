#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// The navigation the shell performs for one Telegram link.
///
/// This is a UI-facing vocabulary, not a second link format: every value below
/// is derived from a DeepLink produced by parseDeepLink() (DeepLinks.h).
enum class DeepLinkAction {
    None,           ///< empty, reset or unrecognised link
    OpenChat,       ///< open a public chat by username
    JoinChat,       ///< ask the user to join a private chat by invite hash
    OpenMessage,    ///< open one post inside a public chat
    OpenProfile,    ///< open a user profile by numeric id
    OpenStickerSet, ///< reserved, never produced today (see the class note)
    OpenBot,        ///< open a bot chat; no /start payload is sent
    ConfirmPhone,   ///< ask the user to confirm a phone number
    OpenSettings,   ///< open the Telegram settings page
};

/// Turns one pasted or clicked link into a single action the shell can perform.
///
/// The router owns no parsing rules. setLink() hands the text to
/// parseDeepLink() and maps the returned DeepLink::Kind, so the two can never
/// disagree about what a link means:
///
///   DeepLink::Kind::PublicChat   -> OpenChat        target = username
///   DeepLink::Kind::Message      -> OpenMessage     target = username, messageId
///   DeepLink::Kind::BotStart     -> OpenBot         target = username
///   DeepLink::Kind::Invite       -> JoinChat        target = invite hash
///   DeepLink::Kind::Phone        -> ConfirmPhone    target = "+<digits>"
///   DeepLink::Kind::PrivateChat  -> OpenProfile     target = decimal user id
///   DeepLink::Kind::Settings     -> OpenSettings    target = empty
///   DeepLink::Kind::Unknown      -> None            target = empty
///
/// Target rules follow the parser's own field semantics: a public username is
/// lower-cased because Telegram usernames are case-insensitive, while an invite
/// hash, a phone number and a numeric id are kept verbatim because their case
/// and leading '+' are significant. messageId is carried only for message
/// links; every other action reports 0.
///
/// DeepLinkAction::OpenStickerSet is never returned: the parser treats
/// /addstickers and /addemoji as reserved usernames and yields Kind::Unknown,
/// and the router deliberately does not re-parse a link the parser rejected.
/// The enumerator exists so a future sticker-set kind can be mapped without an
/// API change.
///
/// A BotStart link becomes OpenBot and its /start payload is dropped: the
/// router only names the chat to open, and the caller reads the payload from
/// parseDeepLink() when it wants to offer it. Nothing is ever sent automatically.
///
/// Link text rules: the stored link is the trimmed input, so link() returns what
/// the user pasted minus surrounding whitespace. "http://" on a Telegram host
/// and a bare "t.me/", "www.t.me/", "telegram.me/" or "www.telegram.me/" prefix
/// are rewritten to the canonical "https://" form before parsing, because the
/// shell receives those forms from web pages and other chat clients. That
/// rewrite only touches the scheme; the host, path and query are still decided
/// by the parser, which keeps rejecting foreign hosts, unknown routes and
/// malformed queries. It is safe here because the router never fetches the URL.
///
/// changed() is emitted only when the stored link, action, target or message id
/// actually differ from the previous ones, so a repeated or re-pasted link
/// causes no repaint.
class DeepLinkRouter final : public QObject {
    Q_OBJECT

public:
    explicit DeepLinkRouter(QObject* parent = nullptr);

    /// Parses and routes one link. An empty or whitespace-only link is the same
    /// as reset(); an unrecognised link is stored as-is with action None, an
    /// empty target and messageId 0, so link() still shows what was rejected.
    void setLink(const QString& url);

    /// The trimmed link last passed to setLink(), or empty after reset().
    [[nodiscard]] QString link() const { return m_link; }

    /// The action for the current link, or None when it was not recognised.
    [[nodiscard]] DeepLinkAction action() const { return m_action; }

    /// Username, invite hash, phone number, numeric user id or sticker set name,
    /// depending on action(); empty for None and OpenSettings.
    [[nodiscard]] QString target() const { return m_target; }

    /// The post id of a message link; 0 for every other action.
    [[nodiscard]] qint64 messageId() const { return m_messageId; }

    /// True when the current link produced an action, i.e. action() != None.
    [[nodiscard]] bool valid() const { return m_action != DeepLinkAction::None; }

    /// Drops the link and the derived state. Does nothing, and stays silent,
    /// when the router is already empty.
    void reset();

signals:
    /// Emitted whenever link(), action(), target() or messageId() changed.
    void changed();

private:
    QString m_link;
    DeepLinkAction m_action = DeepLinkAction::None;
    QString m_target;
    qint64 m_messageId = 0;
};

} // namespace beygram::telegram
