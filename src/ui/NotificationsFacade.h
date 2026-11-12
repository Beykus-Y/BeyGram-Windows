#pragma once

#include "telegram/ChatNotificationController.h"
#include "telegram/NotificationSettingsModel.h"
#include "telegram/NotificationSoundModel.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// The notifications settings screen as one QML-facing object.
///
/// This is a thin presentation facade, not a second source of truth: the global
/// switches and the quiet-hours window live in NotificationSettingsModel, the
/// offered sounds and the selection in NotificationSoundModel, and the per-chat
/// overrides in ChatNotificationController. Every property is read straight
/// from those collaborators and their changed() signals are forwarded as this
/// object's changed(), so a binding repaints on a real update and stays quiet on
/// a redundant one (each model already suppresses no-op writes).
///
/// The facade only does what QML cannot:
///   - it accepts the string lists TDLib-shaped payloads arrive as and turns
///     them into SoundOption values,
///   - it formats the quiet-hours window for the screen,
///   - it translates a sound row index to the model's sound id, and
///   - it keeps the loaded chat id as text so saveChat() can hand it back
///     without a JavaScript number losing precision.
///
/// Formatting:
///   - quietText() is "Off" while no window is configured, which is how the
///     model spells "quiet hours off" (start equals end), and
///     "22:00 – 08:00" otherwise. Both bounds are printed zero-padded on a
///     24-hour clock and separated by a spaced en dash. A window that wraps
///     around midnight is printed as entered, start first, because that is how
///     the user reads "quiet from 22:00 until 08:00".
///   - soundTitles() lists the stored sounds only. "Silent" is not a stored
///     sound in the model (it is the selection being 0), so it is not a row and
///     soundIndex() reports -1 for it.
///
/// Invariants:
///   - soundIndex() is the position in soundTitles() of the selected sound, or
///     -1 when the selection is silent or names a sound the current list does
///     not store. The model keeps the id across a refreshed list, so a list
///     that brings the sound back restores the same index without the facade
///     remembering anything.
///   - selectSound() takes that same index: a negative index selects silent, an
///     index inside soundTitles() selects that sound and an index past the end
///     is ignored, so a stale row cannot select a sound that is not offered.
///   - setSounds() reads ids and titles positionally and a missing title reads
///     as empty. An id that is not a positive number is dropped by the model,
///     and the first occurrence of an id wins, as NotificationSoundModel
///     specifies.
///   - loadChat() takes the chat id as text. A value that is not a number, or
///     the id 0 Telegram never assigns, is not a chat: the overrides are reset
///     to the controller's defaults and the setters and saveChat() stay inert,
///     so a screen that failed to load a chat cannot edit or save another one.
///   - saveChat() is the only place a chat edit leaves the facade. It emits
///     chatSettingsRequested() with the loaded chat id and the current flags,
///     then accepts those flags as the saved baseline. A clean chat emits
///     nothing, so a double click cannot send the same request twice.
///
/// The facade never talks to TDLib, never logs, performs no I/O and never reads
/// the clock; the caller sends setChatNotificationSettings and reports the
/// outcome by loading the chat again.
class NotificationsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool sound READ sound WRITE setSound NOTIFY changed)
    Q_PROPERTY(bool preview READ preview WRITE setPreview NOTIFY changed)
    Q_PROPERTY(bool dnd READ dnd WRITE setDnd NOTIFY changed)
    Q_PROPERTY(int quietStart READ quietStart NOTIFY changed)
    Q_PROPERTY(int quietEnd READ quietEnd NOTIFY changed)
    Q_PROPERTY(QString quietText READ quietText NOTIFY changed)
    Q_PROPERTY(QStringList soundTitles READ soundTitles NOTIFY changed)
    Q_PROPERTY(int soundIndex READ soundIndex NOTIFY changed)
    Q_PROPERTY(bool chatMuted READ chatMuted NOTIFY changed)
    Q_PROPERTY(bool chatSound READ chatSound NOTIFY changed)
    Q_PROPERTY(bool chatPreview READ chatPreview NOTIFY changed)

public:
    explicit NotificationsFacade(QObject* parent = nullptr);

    /// Whether notifications are on for the whole account. On by default.
    [[nodiscard]] bool enabled() const;
    void setEnabled(bool on);

    /// Whether a notification may play a sound. On by default. It does not
    /// decide whether a notification is shown.
    [[nodiscard]] bool sound() const;
    void setSound(bool on);

    /// Whether a notification may show the sender and the message text. On by
    /// default.
    [[nodiscard]] bool preview() const;
    void setPreview(bool on);

    /// Whether do-not-disturb is on. Off by default.
    [[nodiscard]] bool dnd() const;
    void setDnd(bool on);

    /// The start of the quiet-hours window in minutes of the day, 0..1439.
    [[nodiscard]] int quietStart() const;

    /// The end of the quiet-hours window in minutes of the day, 0..1439.
    [[nodiscard]] int quietEnd() const;

    /// "22:00 – 08:00", or "Off" while no window is configured.
    [[nodiscard]] QString quietText() const;

    /// Installs the quiet-hours window as minutes of the day. Both bounds are
    /// clamped to 0..1439 by the model, and equal bounds turn the window off.
    /// A start later than the end wraps around midnight.
    Q_INVOKABLE void setQuietHours(int startMinutes, int endMinutes);

    /// Replaces the offered sounds from the parallel string lists TDLib-shaped
    /// payloads arrive as: ids are decimal integers and titles are free text.
    /// The model drops non-positive and duplicate ids; the selection is kept.
    Q_INVOKABLE void setSounds(const QStringList& ids, const QStringList& titles);

    /// The titles of the stored sounds, in display order.
    [[nodiscard]] QStringList soundTitles() const;

    /// The row of the selected sound in soundTitles(), or -1 for silent or a
    /// selection the current list does not store.
    [[nodiscard]] int soundIndex() const;

    /// Selects a sound by its row in soundTitles(). A negative index selects
    /// silent and an index past the end is ignored.
    Q_INVOKABLE void selectSound(int index);

    /// Loads a chat and its persisted overrides as the clean baseline. A chat
    /// id that is not a number, or 0, loads no chat and resets the overrides.
    Q_INVOKABLE void loadChat(const QString& chatId, bool muted, bool sound, bool preview);

    [[nodiscard]] bool chatMuted() const;
    [[nodiscard]] bool chatSound() const;
    [[nodiscard]] bool chatPreview() const;

    /// Edits one override of the loaded chat. Ignored while no chat is loaded.
    Q_INVOKABLE void setChatMuted(bool muted);
    Q_INVOKABLE void setChatSound(bool on);
    Q_INVOKABLE void setChatPreview(bool on);

    /// Emits chatSettingsRequested() with the loaded chat id and the current
    /// overrides when the chat is dirty, then marks them saved. A clean chat or
    /// no loaded chat emits nothing.
    Q_INVOKABLE void saveChat();

signals:
    /// Emitted whenever any bound model actually changed.
    void changed();

    /// A chat edit to send as setChatNotificationSettings, for the loaded chat
    /// and the flags the user chose.
    void chatSettingsRequested(const QString& chatId, bool muted, bool sound, bool preview);

private:
    telegram::NotificationSettingsModel m_settings;
    telegram::NotificationSoundModel m_sounds;
    telegram::ChatNotificationController m_chat;
    QString m_chatId;
};

} // namespace beygram::ui
