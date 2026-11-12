#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One selectable notification sound, as the sound picker needs it: the TDLib
/// file id of the sound, the label shown in the list and the length of the
/// clip in milliseconds.
///
/// A soundId of 0 is not a sound; it is the "silent" entry the caller offers
/// itself and never appears in the stored options.
struct SoundOption {
    qint64 soundId = 0;
    QString title;
    int durationMs = 0;
};

/// Holds the list of notification sounds the settings screen can offer and the
/// one the user picked.
///
/// The model is a plain value holder: it stores already-parsed SoundOption
/// values, never talks to TDLib, performs no I/O and does not log. The caller
/// decides what the sound ids mean and what to do with the selected one.
///
/// Invariants:
///
///  - A sound id identifies an option, so a non-positive id is not a sound and
///    is dropped on assignment. A later option repeating an already stored id
///    is dropped as well: the first copy wins and the incoming order is
///    otherwise preserved.
///  - A duration is a length of time and cannot be negative, so a negative
///    durationMs is stored as 0 instead of a value no clip can have.
///  - The selection is an id, not an index, and 0 means "silent": no sound is
///    played and selected() reports a default SoundOption. select() accepts 0
///    or an id that is currently stored; an unknown id is ignored so the
///    caller cannot select a sound that is not in the list.
///  - The selection survives a new option list. If the selected id is no
///    longer stored, selectedId() still reports it (so a refreshed list that
///    brings the sound back keeps the user's choice), while selected() falls
///    back to the default and selectedTitle() reports an empty title because
///    no stored option carries that id.
///  - changed() is emitted only when a value really differs. Assigning the
///    same options, reselecting the stored id or clearing an already empty
///    model is silent, so a refresh does not repaint the screen.
class NotificationSoundModel final : public QObject {
    Q_OBJECT

public:
    explicit NotificationSoundModel(QObject* parent = nullptr);

    /// Replaces the stored sounds, dropping non-positive ids, later options
    /// repeating a stored id and clamping negative durations to 0. Emits
    /// changed() only when the resulting list differs from the stored one.
    /// The selection is left untouched.
    void setOptions(QVector<SoundOption> options);

    /// Returns the stored sounds in order.
    [[nodiscard]] QVector<SoundOption> options() const;

    /// Returns the number of stored sounds.
    [[nodiscard]] int count() const;

    /// Selects the sound with `soundId`. 0 selects "silent", an id that is
    /// stored selects that sound and any other id is ignored. Emits changed()
    /// only when the selected id actually differs.
    void select(qint64 soundId);

    /// Returns the selected sound id, or 0 when silent. The id may name a
    /// sound that is no longer stored.
    [[nodiscard]] qint64 selectedId() const;

    /// Returns the selected sound, or a default SoundOption when silent or
    /// when the selected id is not stored.
    [[nodiscard]] SoundOption selected() const;

    /// Returns the title of the selected sound, "Silent" when the selection is
    /// 0 and an empty string when the selected id is not stored.
    [[nodiscard]] QString selectedTitle() const;

    /// Drops the stored sounds and the selection, emitting changed() only when
    /// there was something to drop.
    void clear();

signals:
    /// Emitted once per real change to the sounds or the selection.
    void changed();

private:
    QVector<SoundOption> m_options;
    qint64 m_selected = 0;
};

} // namespace beygram::telegram
