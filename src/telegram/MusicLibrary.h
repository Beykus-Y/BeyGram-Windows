#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One track of the local music library.
///
/// id identifies the track (normally the Telegram message it came from),
/// title, performer and album are the already-parsed tags (any may be empty),
/// durationSeconds is the track length and fileId the Telegram file holding the
/// audio, or 0 while no file is known. All fields are already-parsed values;
/// nothing here touches TDLib.
struct LibraryTrack {
    qint64 id = 0;
    QString title;
    QString performer;
    QString album;
    int durationSeconds = 0;
    int fileId = 0;
};

/// Holds the tracks of the local music library.
///
/// The library is a plain value holder: it stores already-parsed LibraryTrack
/// values and answers search and grouping queries without touching TDLib or the
/// UI. Tracks without a positive id are dropped on assignment because they
/// cannot be addressed, a negative duration is clamped to zero, and later tracks
/// repeating an already stored id are dropped so every track appears once; the
/// first track wins and the incoming order is otherwise kept.
class MusicLibrary final : public QObject {
    Q_OBJECT

public:
    explicit MusicLibrary(QObject* parent = nullptr);

    /// Replaces the stored tracks, dropping tracks without a positive id,
    /// clamping negative durations to zero and dropping later tracks whose id is
    /// already stored. Emits changed() only when the resulting list differs from
    /// the stored one.
    void setTracks(QVector<LibraryTrack> tracks);

    /// Returns a copy of the stored tracks in stored order.
    [[nodiscard]] QVector<LibraryTrack> tracks() const;

    /// Returns the number of stored tracks.
    [[nodiscard]] int count() const;

    /// Returns the tracks whose title, performer or album contains query,
    /// ignoring case, in stored order. A query that is empty after trimming
    /// matches every stored track.
    [[nodiscard]] QVector<LibraryTrack> search(const QString& query) const;

    /// Returns the distinct non-empty performers, sorted ignoring case. Values
    /// that differ only in case are one performer; the first stored spelling is
    /// kept.
    [[nodiscard]] QStringList performers() const;

    /// Returns the distinct non-empty albums, sorted ignoring case. Values that
    /// differ only in case are one album; the first stored spelling is kept.
    [[nodiscard]] QStringList albums() const;

    /// Returns the tracks whose performer equals performer, ignoring case, in
    /// stored order. An empty performer matches nothing.
    [[nodiscard]] QVector<LibraryTrack> byPerformer(const QString& performer) const;

    /// Drops the tracks, emitting changed() when there was anything to drop.
    void clear();

signals:
    void changed();

private:
    QVector<LibraryTrack> m_tracks;
};

} // namespace beygram::telegram
