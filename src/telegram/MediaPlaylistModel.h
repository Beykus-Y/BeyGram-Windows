#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One item of the media player queue: the message that carries the media, the
/// chat it belongs to, and the labels the player shows for it.
struct PlaylistEntry {
    qint64 messageId = 0;
    qint64 chatId = 0;
    QString title;
    int durationSeconds = 0;
};

/// The queue behind the media player.
///
/// A message id identifies an entry, so setEntries() keeps the first copy of a
/// repeated id and drops entries that cannot address a message (a non-positive
/// message id or chat id). Setting entries stops playback: the index goes back
/// to -1 while repeatAll() and shuffle() keep their values, so replacing the
/// queue never silently starts a track.
///
/// playAt() accepts only an index inside the queue. next() and previous() step
/// through the queue in order and wrap around at either end only when
/// repeatAll() is set; otherwise they report that there is no further track and
/// keep the current index. With no current track (-1) next() starts at the
/// first entry and previous() at the last one.
///
/// shuffle() is stored as a preference for the player and does not reorder the
/// queue, so navigation stays deterministic.
///
/// Every mutation that changes the stored state emits changed(); a call that
/// leaves the queue and the index as they were stays silent.
class MediaPlaylistModel final : public QObject {
    Q_OBJECT

public:
    explicit MediaPlaylistModel(QObject* parent = nullptr);

    /// Replaces the queue with a de-duplicated copy of \a entries and clears the
    /// current index.
    void setEntries(QVector<PlaylistEntry> entries);
    [[nodiscard]] QVector<PlaylistEntry> entries() const;
    [[nodiscard]] int count() const;
    /// Index of the current entry, or -1 when no entry is current.
    [[nodiscard]] int currentIndex() const;
    /// The current entry, or a default PlaylistEntry when the index is -1.
    [[nodiscard]] PlaylistEntry current() const;
    [[nodiscard]] bool repeatAll() const;
    [[nodiscard]] bool shuffle() const;
    void setRepeatAll(bool on);
    void setShuffle(bool on);
    /// Makes \a index current; an index outside the queue is ignored.
    void playAt(int index);
    /// Moves to the following entry. Returns false at the end of the queue
    /// unless repeatAll() is set, in which case it wraps to the first entry.
    bool next();
    /// Moves to the preceding entry. Returns false at the start of the queue
    /// unless repeatAll() is set, in which case it wraps to the last entry.
    bool previous();
    /// Drops every entry and the current index; repeatAll() and shuffle() stay.
    void clear();

signals:
    void changed();

private:
    QVector<PlaylistEntry> m_entries;
    int m_index = -1;
    bool m_repeat = false;
    bool m_shuffle = false;
};

} // namespace beygram::telegram
