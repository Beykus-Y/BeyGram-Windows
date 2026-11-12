#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {
class SecretChatSafetyController;
class SpoilerRevealModel;
} // namespace beygram::telegram

namespace beygram::ui {

/// QML-facing facade over the secret-chat safety state and the spoiler reveals
/// of the chat currently open.
///
/// The facade owns its controllers and exposes them as one object with one
/// changed() signal, so a view binds a handful of properties instead of
/// reaching into two C++ objects. It holds no transport and performs no
/// logging: a new self-destruct timer is only announced through ttlRequested()
/// for the caller to send.
///
/// State comes from two places:
///  - SecretChatSafetyController holds the loaded chat, whether it is secret,
///    its TTL and whether screenshots are allowed. Its derived rules are
///    forwarded as-is, so a secret chat never notifies and never copies.
///  - SpoilerRevealModel holds the transient, per-view reveal state. Reveals
///    are never persisted and never sent; they only last until the message is
///    covered again, the cap evicts it or another chat is loaded.
///
/// Protected content is a chat flag the safety controller does not model, so
/// the facade stores it next to the controller. It is the "content is
/// protected" flag of a chat or channel: while it is set, copying is denied
/// even in a chat that would otherwise allow it. Unloading a chat drops the
/// flag together with the rest of the chat's state.
///
/// Invariants:
///  - secret() is true exactly while a secret chat is loaded.
///  - ttlSeconds() is the clamped TDLib value (0..604800) and ttlText() renders
///    it for display, "Off" when the timer is off.
///  - copyAllowed() is false for a secret chat and for protected content.
///  - screenshotWarn() is true only for a secret chat that still allows
///    screenshots, i.e. exactly when the other side would be told about one.
///  - ttlRequested() is emitted only for a loaded chat, and carries the
///    clamped value that is already stored here. Every explicit pick is
///    announced, even when the value is already in effect; changed() stays
///    silent in that case.
class SecurityFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool secret READ secret NOTIFY changed)
    Q_PROPERTY(int ttlSeconds READ ttlSeconds NOTIFY changed)
    Q_PROPERTY(QString ttlText READ ttlText NOTIFY changed)
    Q_PROPERTY(bool notificationsAllowed READ notificationsAllowed NOTIFY changed)
    Q_PROPERTY(bool copyAllowed READ copyAllowed NOTIFY changed)
    Q_PROPERTY(bool screenshotWarn READ screenshotWarn NOTIFY changed)
    Q_PROPERTY(bool protectedContent READ protectedContent NOTIFY changed)

public:
    explicit SecurityFacade(QObject* parent = nullptr);

    [[nodiscard]] bool secret() const;
    [[nodiscard]] int ttlSeconds() const;

    /// The loaded chat's timer as a short label: "Off" for 0, otherwise the
    /// largest whole unit that fits ("45 seconds", "5 minutes", "2 hours",
    /// "7 days"). A non-whole value is truncated to that unit, because the
    /// selector never offers anything else.
    [[nodiscard]] QString ttlText() const;

    [[nodiscard]] bool notificationsAllowed() const;
    [[nodiscard]] bool copyAllowed() const;
    [[nodiscard]] bool screenshotWarn() const;
    [[nodiscard]] bool protectedContent() const;

    /// Loads `chatId` as the open chat. An empty, non-numeric or zero id
    /// unloads the current chat, which also drops its TTL, screenshot flag,
    /// protected-content flag and every reveal. Negative ids are real chats
    /// (groups and channels) and are kept as they are.
    ///
    /// The TTL and screenshot flag are not touched by a load, because the
    /// caller reads them from TDLib right after the chat; loading a different
    /// chat covers every spoiler, because a reveal belongs to the chat that
    /// showed it.
    Q_INVOKABLE void setChat(const QString& chatId, bool secret, bool protectedContent);

    /// Applies a new self-destruct timer and announces it through
    /// ttlRequested() so the caller can send it. Out-of-range values are
    /// clamped to TDLib's 0..604800 rather than rejected, and the request
    /// carries the clamped value. Ignored while no chat is loaded.
    Q_INVOKABLE void setTtl(int seconds);

    /// Mirrors whether the loaded chat may be screenshotted. Only a secret chat
    /// can have this turned off, and turning it off also removes the warning.
    Q_INVOKABLE void setScreenshotAllowed(bool allowed);

    /// Reveals one message's spoiler. Unknown, zero and negative ids are
    /// ignored, and revealing an already revealed message changes nothing.
    Q_INVOKABLE void reveal(const QString& messageId);

    /// Covers every message again, e.g. when the chat is closed.
    Q_INVOKABLE void hideAllSpoilers();

    [[nodiscard]] bool isRevealed(const QString& messageId) const;

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted when the user picked a new self-destruct timer for the loaded
    /// chat. `seconds` is the clamped value and is already stored here. Every
    /// explicit pick is announced, including one that keeps the current value;
    /// changed() is what stays silent then.
    void ttlRequested(const QString& chatId, int seconds);

private:
    telegram::SecretChatSafetyController* m_safety = nullptr;
    telegram::SpoilerRevealModel* m_spoilers = nullptr;
    bool m_protected = false;
};

} // namespace beygram::ui
