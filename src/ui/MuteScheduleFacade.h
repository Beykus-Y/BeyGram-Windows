#pragma once

#include "telegram/ChatMuteSchedule.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::ui {

/// The per-chat mute sheet as one QML-facing object.
///
/// Presentation and command translation only: the durations and the formatting
/// of a running mute belong to ChatMuteSchedule, which never reads the clock.
/// This facade supplies the clock when it has to (the current label and the
/// expiry a chosen duration means) and turns the loaded chat into a
/// muteRequested() payload for the controller to persist.
///
/// A chat is loaded with setChat(chatId, mutedUntil); the id is kept as the
/// text the caller passed (trimmed) so a 64-bit Telegram id survives the trip
/// through QML unchanged. Only one chat is loaded at a time: loading another
/// one forgets the previous record, and clear() unloads the chat entirely.
///
/// Invariants:
///  - A chat id that is not a positive decimal integer loads no chat. The
///    facade drops the previous one and every mutator stays inert, so a sheet
///    that failed to load cannot mute another chat.
///  - mutedUntil is -1 (forever), 0 (unmuted) or a positive Unix second. A
///    value below -1 is rejected and leaves the loaded chat untouched, matching
///    ChatMuteSchedule.
///  - label() and muted() are evaluated at the current wall clock. A timed mute
///    that has already expired reads "Unmuted"/false even before any write.
///  - options() is the fixed list of tokens choose() accepts, in sheet order:
///    "1h", "8h", "1d", "1w", "forever", "off". It is not translated; the QML
///    sheet maps each token to its display text.
///  - choose() with no loaded chat or an unknown token is silent. A recognized
///    token that moves the stored expiry emits muteRequested() with the loaded
///    chat id and the expiry, then changed(). A choice that matches the stored
///    expiry is a no-op and emits nothing, so a repeated tap cannot send the
///    same request twice.
///  - setChat() emits changed() when the loaded chat or its stored expiry
///    moves. Loading the same chat with the same expiry is silent.
///  - clear() unloads the chat and forgets its record; it emits changed() once
///    when a chat was loaded and never emits muteRequested().
///
/// The facade performs no I/O, never logs and never talks to TDLib.
class MuteScheduleFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString label READ label NOTIFY changed)
    Q_PROPERTY(bool muted READ muted NOTIFY changed)
    Q_PROPERTY(QStringList options READ options NOTIFY changed)

public:
    explicit MuteScheduleFacade(QObject* parent = nullptr);

    /// The mute state of the loaded chat for a row or a sheet header:
    /// "Muted forever", "Muted for 2h", "Muted for 3d" or "Unmuted". With no
    /// chat loaded this is "Unmuted".
    [[nodiscard]] QString label() const;

    /// Whether the loaded chat is muted at the current wall clock. False with
    /// no chat loaded.
    [[nodiscard]] bool muted() const;

    /// The choose() tokens the sheet offers, in display order.
    [[nodiscard]] QStringList options() const;

    /// Loads a chat and its stored mute expiry. An id that is not a positive
    /// decimal integer unloads the current chat; an expiry below -1 is rejected
    /// and leaves the loaded chat unchanged. Emits changed() once when the
    /// loaded chat or its stored expiry really moves.
    Q_INVOKABLE void setChat(const QString& chatId, qint64 mutedUntil);

    /// Applies one option token ("1h", "8h", "1d", "1w", "forever" or "off")
    /// to the loaded chat. A token that moves the stored expiry emits
    /// muteRequested() with the expiry it means at the current wall clock and
    /// then changed(). An unknown token, no loaded chat, or a token that
    /// matches the stored expiry is silent.
    Q_INVOKABLE void choose(const QString& option);

    /// Unloads the chat and forgets its record. Emits changed() once when a
    /// chat was loaded; never emits muteRequested().
    Q_INVOKABLE void clear();

signals:
    /// Emitted when the loaded chat or its stored mute expiry really changes.
    void changed();

    /// The mute edit to persist: the loaded chat id as text and the Unix
    /// second the mute ends at (-1 forever, 0 unmuted).
    void muteRequested(const QString& chatId, qint64 untilUnix);

private:
    telegram::ChatMuteSchedule m_schedule;
    QString m_chatId;
    qint64 m_chatKey = 0;
};

} // namespace beygram::ui
