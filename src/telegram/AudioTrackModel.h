#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One audio track taken from a chat's shared-media list.
///
/// messageId is the Telegram message the track came from and identifies it,
/// title and performer are the already-parsed tags (either may be empty),
/// durationSeconds the track length and fileName the downloaded file's name.
/// All fields are already-parsed values; nothing here touches TDLib.
struct AudioTrack {
    qint64 messageId = 0;
    QString title;
    QString performer;
    int durationSeconds = 0;
    QString fileName;
};

/// Holds the audio tracks of one chat.
///
/// The model is a plain value holder: it stores already-parsed AudioTrack values
/// and answers performer, title and duration queries without touching TDLib or
/// the UI. Tracks without a positive messageId are dropped on assignment
/// because they cannot be played or addressed, a negative duration is clamped
/// to zero, and later tracks repeating an already stored messageId are dropped
/// so every message appears once; the first track wins and the incoming order
/// is otherwise kept.
class AudioTrackModel final : public QObject {
    Q_OBJECT

public:
    explicit AudioTrackModel(QObject* parent = nullptr);

    /// Replaces the stored tracks, dropping tracks without a positive
    /// messageId, clamping negative durations to zero and dropping later tracks
    /// whose messageId is already stored. Emits changed() only when the
    /// resulting list differs from the stored one.
    void setTracks(QVector<AudioTrack> tracks);

    /// Returns a copy of the stored tracks in stored order.
    [[nodiscard]] QVector<AudioTrack> tracks() const;

    /// Returns the number of stored tracks.
    [[nodiscard]] int count() const;

    /// Returns the tracks whose performer equals performer, ignoring case, in
    /// stored order. An empty performer matches nothing.
    [[nodiscard]] QVector<AudioTrack> byPerformer(const QString& performer) const;

    /// Returns the display label of the track at index: "Performer — Title",
    /// or the one that is present when the other is empty. Returns
    /// "Unknown title" when both are empty, and an empty string when index is
    /// out of range.
    [[nodiscard]] QString displayTitle(int index) const;

    /// Returns the sum of every stored track's duration in seconds.
    [[nodiscard]] int totalDuration() const;

    /// Drops the tracks, emitting changed() when there was anything to drop.
    void clear();

signals:
    void changed();

private:
    QVector<AudioTrack> m_tracks;
};

} // namespace beygram::telegram
