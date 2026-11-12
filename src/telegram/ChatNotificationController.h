#pragma once

#include <QJsonObject>
#include <QObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Edits the per-chat notification overrides of one chat and remembers what was
/// last saved.
///
/// The controller is the state behind the per-chat notification screen:
/// setChat() loads a chat together with its persisted mute/sound/preview flags
/// and takes them as the saved baseline, the three setters edit one flag at a
/// time, and request() turns an unsaved edit into a setChatNotificationSettings
/// request. dirty() therefore means "this screen has something to save".
///
/// Chat id 0 means "no chat loaded" (Telegram never assigns it, but supergroup
/// and channel ids are negative and stay valid): request() is empty and the
/// setters are ignored, so a view that has not loaded a chat cannot produce a
/// request for the wrong chat.
///
/// changed() is emitted whenever the loaded chat, one of the flags or dirty()
/// changes, so a view can bind its fields to the getters and its save button to
/// dirty() without repainting for no-ops.
class ChatNotificationController final : public QObject {
    Q_OBJECT

public:
    explicit ChatNotificationController(QObject* parent = nullptr);

    /// Loads `chatId` with its persisted flags and makes them the saved
    /// baseline, so a freshly loaded chat is never dirty. Any pending edit of
    /// the previous chat is discarded, not carried over.
    void setChat(qint64 chatId, bool muted, bool sound, bool preview);

    /// Edits one flag of the loaded chat. Ignored while no chat is loaded, and a
    /// value that is already stored emits nothing.
    void setMuted(bool muted);
    void setSound(bool sound);
    void setPreview(bool preview);

    [[nodiscard]] qint64 chatId() const { return m_chatId; }
    [[nodiscard]] bool muted() const { return m_muted; }
    [[nodiscard]] bool sound() const { return m_sound; }
    [[nodiscard]] bool preview() const { return m_preview; }

    /// True while a chat is loaded and at least one flag differs from the saved
    /// baseline. Reverting an edit by hand clears it again.
    [[nodiscard]] bool dirty() const;

    /// setChatNotificationSettings for the loaded chat, or an empty object when
    /// no chat is loaded or nothing changed.
    ///
    /// The mute is written as this chat's own mute, so "muted" means muted until
    /// the user unmutes; the mute is never inherited from the chat's scope.
    /// mentionsOnly is not modelled by this controller and is written as false,
    /// i.e. mentions notify like any other message.
    [[nodiscard]] QJsonObject request() const;

    /// Accepts the current flags as the saved baseline, e.g. after TDLib
    /// confirmed the request. Emits changed() when that clears a dirty state.
    void markSaved();

signals:
    /// Emitted whenever the loaded chat, one of the flags or dirty() changed.
    void changed();

private:
    qint64 m_chatId = 0;
    bool m_muted = false, m_sound = true, m_preview = true;
    bool o_muted = false, o_sound = true, o_preview = true;
};

} // namespace beygram::telegram
