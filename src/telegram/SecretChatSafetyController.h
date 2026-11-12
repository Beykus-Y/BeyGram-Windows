#pragma once

#include <QObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Safety state of the chat currently open: whether it is a secret chat, how
/// long its messages live and whether screenshots are allowed.
///
/// The controller is the state behind the chat's secret-chat screen. setChat()
/// loads a chat, setTtlSeconds() and setScreenshotAllowed() mirror what TDLib
/// reported or what the user picked, and the const getters answer what the rest
/// of the app may do. It stores values only: no request is built, nothing is
/// sent and nothing is logged.
///
/// The three derived getters are the reason the class exists, because they state
/// the secret-chat rules once instead of in every view:
///   - notificationsAllowed() is false for a secret chat and true for every
///     other chat, whatever the flags hold: secret chats never notify.
///   - copyAllowed() is false for a secret chat and true for every other chat.
///   - shouldWarnBeforeScreenshot() is true only for a secret chat that still
///     allows screenshots, i.e. exactly the case where the user can take one but
///     the other side is told about it.
///
/// chatId() 0 means "no chat loaded": setChat(0, ...) is clear(), so unloading a
/// chat cannot leave its TTL or screenshot flag behind. Negative ids are real
/// chats (groups, channels) and are kept as they are.
///
/// setChat() changes only the identity of the loaded chat. The TTL and the
/// screenshot flag keep their values, because they are read from TDLib right
/// after the chat and are never guessed for it; clear() is what drops them.
///
/// changed() is emitted only when a stored value really changed, so a view can
/// bind its controls to the getters and repaint on the signal alone.
class SecretChatSafetyController final : public QObject {
    Q_OBJECT

public:
    explicit SecretChatSafetyController(QObject* parent = nullptr);

    /// Loads `chatId` as a secret chat when `secret` is true and as a regular
    /// chat otherwise. Chat id 0 unloads the current chat, exactly like clear().
    void setChat(qint64 chatId, bool secret);

    [[nodiscard]] qint64 chatId() const { return m_chatId; }
    [[nodiscard]] bool secret() const { return m_secret; }

    /// Self-destruct timer of the loaded chat, in seconds. Values outside
    /// 0..604800 (TDLib's one-week maximum) are clamped rather than rejected,
    /// and 0 means the timer is off.
    void setTtlSeconds(int seconds);
    [[nodiscard]] int ttlSeconds() const { return m_ttl; }

    /// Whether the loaded chat may be screenshotted. Defaults to true, which is
    /// TDLib's default; only a secret chat can have it turned off.
    void setScreenshotAllowed(bool allowed);
    [[nodiscard]] bool screenshotAllowed() const { return m_screenshots; }

    /// True when a screenshot is possible but the other side will notice: a
    /// secret chat that still allows screenshots. Regular chats never warn.
    [[nodiscard]] bool shouldWarnBeforeScreenshot() const;

    /// False for a secret chat, true for every other chat. Neither the
    /// screenshot flag nor the TTL affects this.
    [[nodiscard]] bool notificationsAllowed() const;

    /// False for a secret chat, true for every other chat.
    [[nodiscard]] bool copyAllowed() const;

    /// Restores every value to its default: no chat, no TTL, screenshots
    /// allowed. Emits changed() once when anything actually moved, so clearing
    /// an already empty controller stays silent.
    void clear();

signals:
    /// Emitted whenever the loaded chat, its secret flag, the TTL or the
    /// screenshot flag really changed.
    void changed();

private:
    qint64 m_chatId = 0;
    bool m_secret = false, m_screenshots = true;
    int m_ttl = 0;
};

} // namespace beygram::telegram
