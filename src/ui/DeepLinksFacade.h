#pragma once

#include "telegram/DeepLinkRouter.h"

#include <QObject>
#include <QString>

namespace beygram::ui {

/// The resolved Telegram link as one QML-facing object.
///
/// This is a thin presentation facade, not a second parser: the link text, the
/// action, the target and the message id all live in the DeepLinkRouter this
/// class owns, and every property is read straight from it. The facade only
/// does what QML cannot: it turns the router's enum into the stable string
/// vocabulary QML compares against, formats the message id as a string, and
/// re-emits one navigateRequested() signal for the shell to act on.
///
/// The string vocabulary is the only API contract QML sees, so it is spelled
/// out here and never derived from the enumerator names:
///
///   none          -> DeepLinkAction::None           target empty, messageId empty
///   openChat      -> DeepLinkAction::OpenChat       target = username
///   joinChat      -> DeepLinkAction::JoinChat       target = invite hash
///   openMessage   -> DeepLinkAction::OpenMessage    target = username, messageId
///   openProfile   -> DeepLinkAction::OpenProfile    target = decimal user id
///   openBot       -> DeepLinkAction::OpenBot        target = username
///   confirmPhone  -> DeepLinkAction::ConfirmPhone   target = "+<digits>"
///   openSettings  -> DeepLinkAction::OpenSettings   target empty, messageId empty
///   openStickerSet -> DeepLinkAction::OpenStickerSet target = sticker set name
///
/// "openStickerSet" is reserved: the parser rejects /addstickers and /addemoji,
/// so the router never produces that action today. The value exists so a future
/// sticker-set kind can be surfaced without an API change.
///
/// Invariants:
///  - valid() is true exactly when action() != "none".
///  - link() is the trimmed text the user pasted, so a rejected link still
///    shows what was rejected; it is empty after reset().
///  - messageId() is the decimal post id for "openMessage" only, and empty for
///    every other action, including a rejected link.
///  - target() is empty for "none" and "openSettings"; every other action
///    carries the router's value verbatim, case included.
///
/// The facade never touches TDLib, the network or storage, and never fetches
/// the URL. confirm() only reports what the shell should do; opening a chat,
/// joining an invite or dialling a number is the caller's job, and nothing is
/// ever sent automatically.
class DeepLinksFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString link READ link NOTIFY changed)
    Q_PROPERTY(QString action READ action NOTIFY changed)
    Q_PROPERTY(QString target READ target NOTIFY changed)
    Q_PROPERTY(QString messageId READ messageId NOTIFY changed)
    Q_PROPERTY(bool valid READ valid NOTIFY changed)

public:
    explicit DeepLinksFacade(QObject* parent = nullptr);

    /// The trimmed link last passed to setLink(), or empty after reset().
    [[nodiscard]] QString link() const;

    /// The action for the current link as one of the strings listed above.
    [[nodiscard]] QString action() const;

    /// Username, invite hash, phone number, numeric user id or sticker set name,
    /// depending on action(); empty for "none" and "openSettings".
    [[nodiscard]] QString target() const;

    /// The decimal post id of an "openMessage" link; empty for every other
    /// action. The router's 0 ("no message") is formatted as an empty string so
    /// QML never has to treat 0 as a special case.
    [[nodiscard]] QString messageId() const;

    /// True when the current link produced an action, i.e. action() != "none".
    [[nodiscard]] bool valid() const;

    /// Parses and routes one link. An empty or whitespace-only link is the same
    /// as reset(); an unrecognised link is stored as-is with action "none", an
    /// empty target and an empty messageId, so link() still shows what was
    /// rejected. Re-pasting the same link changes nothing and emits nothing.
    Q_INVOKABLE void setLink(const QString& url);

    /// Drops the link and the derived state. Does nothing, and stays silent,
    /// when the facade is already empty.
    Q_INVOKABLE void reset();

    /// Announces the current link through navigateRequested() and returns true
    /// when valid(); returns false and stays silent otherwise.
    ///
    /// The state is deliberately kept after a successful confirm(), because a
    /// navigation the shell cannot perform yet (still connecting, a chat list
    /// still loading) must not lose the user's link; the caller calls reset()
    /// when it is done with it.
    Q_INVOKABLE bool confirm();

signals:
    /// Emitted whenever link(), action(), target(), messageId() or valid()
    /// actually changed.
    void changed();

    /// Emitted by confirm() with the action, target and message id the shell
    /// should navigate to. messageId is empty for every action but "openMessage".
    void navigateRequested(const QString& action, const QString& target,
                           const QString& messageId);

private:
    telegram::DeepLinkRouter m_router;
};

} // namespace beygram::ui
